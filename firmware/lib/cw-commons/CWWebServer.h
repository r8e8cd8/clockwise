#pragma once

#include <WiFi.h>
#include <CWPreferences.h>
#include "StatusController.h"
#include "SettingsWebPage.h"
#include "PokeWebPage.h"
#include "PokeQueue.h"
#include "SceneBridge.h"

#ifndef CLOCKFACE_NAME
  #define CLOCKFACE_NAME "UNKNOWN"
#endif

WiFiServer server(80);

struct ClockwiseWebServer
{
  String httpBuffer;
  bool force_restart;
  bool wifiArm = false;
  bool wifiConnecting = false;
  bool wifiConnectOk = false;
  int wifiChannel = 0;
  unsigned long wifiConnectStarted = 0;
  unsigned long wifiRestartAt = 0;
  String wifiConnectSsid;
  String wifiConnectPwd;
  String wifiLastErr;
  const char* HEADER_TEMPLATE_D = "X-%s: %d\r\n";
  const char* HEADER_TEMPLATE_S = "X-%s: %s\r\n";
 
  static ClockwiseWebServer *getInstance()
  {
    static ClockwiseWebServer base;
    return &base;
  }

  void startWebServer()
  {
    server.begin();
    StatusController::getInstance()->blink_led(100, 3);
  }

  void stopWebServer()
  {
    server.stop();
  }

  static String urlDecode(String s) {
    String out;
    out.reserve(s.length());
    for (unsigned i = 0; i < s.length(); i++) {
      char c = s[i];
      if (c == '+') {
        out += ' ';
      } else if (c == '%' && i + 2 < s.length()) {
        char h[3] = {s[i + 1], s[i + 2], 0};
        out += (char)strtol(h, nullptr, 16);
        i += 2;
      } else {
        out += c;
      }
    }
    return out;
  }

  static String queryGet(const String &query, const String &name) {
    int start = 0;
    while (start < (int)query.length()) {
      int amp = query.indexOf('&', start);
      if (amp < 0) amp = query.length();
      int eq = query.indexOf('=', start);
      if (eq > start && eq < amp) {
        String k = query.substring(start, eq);
        String v = query.substring(eq + 1, amp);
        if (k == name) return urlDecode(v);
      }
      start = amp + 1;
    }
    return "";
  }

  void handleHttpRequest()
  {
    pollWifiConnect();
    if (force_restart)
      StatusController::getInstance()->forceRestart();

    WiFiClient client = server.available();
    if (client)
    {
      StatusController::getInstance()->blink_led(100, 1);

      while (client.connected())
      {
        if (client.available())
        {
          char c = client.read();
          httpBuffer.concat(c);

          if (c == '\n')
          {
            // Must be int — Chinese SSIDs make the request line longer than 255 bytes.
            int method_pos = httpBuffer.indexOf(' ');
            int path_pos = httpBuffer.indexOf(' ', method_pos + 1);
            if (method_pos < 0 || path_pos < 0) {
              httpBuffer = "";
              break;
            }

            String method = httpBuffer.substring(0, method_pos);
            String path = httpBuffer.substring(method_pos + 1, path_pos);
            String key = "";
            String value = "";
            String query = "";

            if (path.indexOf('?') > 0)
            {
              query = path.substring(path.indexOf('?') + 1);
              key = path.substring(path.indexOf('?') + 1, path.indexOf('='));
              value = path.substring(path.indexOf('=') + 1);
              path = path.substring(0, path.indexOf('?'));
            }

            processRequest(client, method, path, key, value, query);
            httpBuffer = "";
            break;
          }
        }
      }
      delay(1);
      client.stop();
    }
  }

  void processRequest(WiFiClient client, String method, String path, String key, String value, String query = "")
  {
    wifi_mode_t mode = WiFi.getMode();
    bool apOn = (mode == WIFI_AP || mode == WIFI_AP_STA) && WiFi.softAPIP() != IPAddress(0, 0, 0, 0);

    // Captive-portal probes (Windows opens /redirect after connecttest).
    bool captiveProbe =
        path == "/" || path == "/generate_204" || path == "/gen_204" ||
        path == "/hotspot-detect.html" || path == "/library/test/success.html" ||
        path == "/connecttest.txt" || path == "/ncsi.txt" || path == "/fwlink" ||
        path == "/redirect" || path == "/success.txt" || path == "/canonical.html" ||
        path.startsWith("/redirect");

    if (method == "GET" && captiveProbe) {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html; charset=utf-8");
      client.println("Connection: close");
      client.println("Cache-Control: no-cache");
      client.println();
      if (!apOn) {
        client.println(SETTINGS_PAGE);
      } else {
      // Simple portal: manual SSID first (scan is optional — scanning used to freeze the page).
      client.println(F(
        "<!DOCTYPE html><html><head><meta charset=utf-8>"
        "<meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>Clockwise WiFi</title>"
        "<style>"
        "body{font-family:sans-serif;padding:16px;background:#e8f0f4;color:#1a2430}"
        "input,button{width:100%;padding:12px;margin:8px 0;font-size:16px;box-sizing:border-box;border-radius:10px;border:1px solid #c5d4e0}"
        "button{background:#2c5f9e;color:#fff;border:0}"
        "button.sec{background:#dce9f7;color:#2c5f9e}"
        "#s{color:#2a7d8c}#list button{background:#fff;color:#1a2430;text-align:left}"
        "</style></head><body>"
        "<h2>Clockwise 配网</h2>"
        "<p id=s>直接填写家里的 <b>2.4G</b> WiFi（不要用 5G）</p>"
        "<input id=ssid placeholder='WiFi 名称 SSID'>"
        "<input id=pwd type=password placeholder='WiFi 密码，开放网络留空'>"
        "<button onclick='go()'>连接</button>"
        "<button class=sec onclick='load()'>搜索附近网络</button>"
        "<div id=list></div>"
        "<p style='font-size:13px;color:#5a6b7a'>本页地址：192.168.4.1</p>"
        "<script>"
        "async function go(){"
        "const ssid=document.getElementById('ssid').value.trim();"
        "if(!ssid){alert('先填 WiFi 名称');return;}"
        "document.getElementById('s').textContent='正在连接…';"
        "const u='/wifi?ssid='+encodeURIComponent(ssid)+'&pwd='+encodeURIComponent(document.getElementById('pwd').value);"
        "try{const t=await(await fetch(u)).text();"
        "document.getElementById('s').textContent=t.indexOf('ok=')>=0?'已提交，请把手机切回家里 WiFi 等 20 秒':t;"
        "}catch(e){document.getElementById('s').textContent='已提交，请切回家里 WiFi';}"
        "}"
        "async function load(){"
        "document.getElementById('s').textContent='搜索中，约 5 秒…';"
        "try{"
        "const t=await(await fetch('/wifi/scan')).text();"
        "const L=document.getElementById('list');L.innerHTML='';"
        "t.split('\\n').forEach(line=>{"
        "if(!line.startsWith('net='))return;const p=line.slice(4).split('|');const n=p[0];if(!n)return;"
        "const b=document.createElement('button');b.className='sec';"
        "b.textContent=n+'  '+(p[2]==='0'?'开放':'需密码');"
        "b.onclick=()=>{document.getElementById('ssid').value=n};"
        "L.appendChild(b);});"
        "document.getElementById('s').textContent=L.children.length?'点选网络，或继续手填':'没搜到，请手填名称';"
        "}catch(e){document.getElementById('s').textContent='搜索失败，请手填 WiFi 名称';}"
        "}"
        "</script></body></html>"
      ));
      }
    } else if (method == "GET" && path == "/poke") {
      String a = query.length() ? queryGet(query, "a") : "";
      if (a.length() == 0) {
        client.println("HTTP/1.0 200 OK");
        client.println("Content-Type: text/html");
        client.println();
        client.println(FPSTR(POKE_PAGE));
      } else {
        String t = queryGet(query, "t");
        if (a == "blink") {
          PokeQueue::get().request(POKE_BLINK);
        } else if (a == "shy") {
          PokeQueue::get().request(POKE_SHY);
        } else if (a == "next") {
          PokeQueue::get().request(POKE_NEXT);
        } else if (a == "surprise") {
          PokeQueue::get().request(POKE_SURPRISE);
        } else if (a == "sleep") {
          PokeQueue::get().request(POKE_SLEEP);
        } else if (a == "wink") {
          PokeQueue::get().request(POKE_WINK);
        } else if (a == "heart") {
          PokeQueue::get().request(POKE_HEART);
        } else if (a == "peek") {
          PokeQueue::get().request(POKE_PEEK);
        } else if (a == "say") {
          PokeQueue::get().request(POKE_SAY, t.c_str());
        } else if (a == "lock") {
          PokeQueue::get().request(POKE_LOCK, t.length() ? t.c_str() : "none");
        } else if (a == "dn" || a == "daynight") {
          PokeQueue::get().request(POKE_DAYNIGHT, t.length() ? t.c_str() : "auto");
        } else if (a == "sec" || a == "seconds") {
          PokeQueue::get().request(POKE_SECONDS, t.length() ? t.c_str() : "toggle");
        } else if (a == "gstart" || a == "gcatch") {
          PokeQueue::get().request(POKE_GAME_CATCH);
        } else if (a == "gsnake") {
          PokeQueue::get().request(POKE_GAME_SNAKE);
        } else if (a == "grhythm") {
          PokeQueue::get().request(POKE_GAME_RHYTHM);
        } else if (a == "gquit") {
          PokeQueue::get().request(POKE_GAME_QUIT);
        } else if (a == "gleft") {
          PokeQueue::get().request(POKE_GAME_LEFT);
        } else if (a == "gright") {
          PokeQueue::get().request(POKE_GAME_RIGHT);
        } else if (a == "gup") {
          PokeQueue::get().request(POKE_GAME_UP);
        } else if (a == "gdown") {
          PokeQueue::get().request(POKE_GAME_DOWN);
        } else if (a == "bye" || a == "leave" || a == "slide") {
          PokeQueue::get().request(POKE_BYE, t.length() ? t.c_str() : "right");
        } else if (a == "back" || a == "come") {
          PokeQueue::get().request(POKE_BACK);
        } else if (a == "call" || a == "knock" || a == "hey") {
          PokeQueue::get().request(POKE_CALL);
        } else if (a == "fav") {
          PokeQueue::get().request(POKE_FAV, t.length() ? t.c_str() : "toggle");
        } else if (a == "bg") {
          PokeQueue::get().request(POKE_BG, t.length() ? t.c_str() : "0");
        } else if (a == "motto" || (a == "say" && t == "motto")) {
          PokeQueue::get().request(POKE_MOTTO);
        } else if (a == "hat" || a == "deco") {
          PokeQueue::get().request(POKE_HAT, t.length() ? t.c_str() : "next");
        }
        client.println("HTTP/1.0 204 No Content");
        client.println();
      }
    } else if (method == "GET" && path == "/bgs") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/plain");
      client.println("Connection: close");
      client.println();
      client.print(SceneBridge::count());
      client.print(',');
      client.print(SceneBridge::current());
    } else if (method == "GET" && path == "/bg") {
      int idx = query.length() ? queryGet(query, "i").toInt() : 0;
      if (idx < 0) idx = 0;
      SceneBridge::writeBmp(client, (uint8_t)idx);
    } else if (method == "GET" && path == "/screen") {
      SceneBridge::writeScreen(client);
    } else if (method == "GET" && path == "/wifi/scan") {
      writeWifiScan(client);
    } else if (method == "GET" && path == "/wifi") {
      handleWifi(client, query);
    } else if (method == "GET" && path == "/get") {
      getCurrentSettings(client);
    } else if (method == "GET" && path == "/read") {
      if (key == "pin") {
        readPin(client, key, value.toInt());
      }
    } else if (method == "POST" && path == "/restart") {
      client.println("HTTP/1.0 204 No Content");
      force_restart = true;
    } else if (method == "POST" && path == "/set") {
      ClockwiseParams::getInstance()->load();
      if (key == ClockwiseParams::getInstance()->PREF_DISPLAY_BRIGHT) {
        ClockwiseParams::getInstance()->displayBright = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_WIFI_SSID) {
        ClockwiseParams::getInstance()->wifiSsid = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_WIFI_PASSWORD) {
        ClockwiseParams::getInstance()->wifiPwd = value;
      } else if (key == "autoBright") {
        ClockwiseParams::getInstance()->autoBrightMin = value.substring(0,4).toInt();
        ClockwiseParams::getInstance()->autoBrightMax = value.substring(5,9).toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_SWAP_BLUE_GREEN) {
        ClockwiseParams::getInstance()->swapBlueGreen = (value == "1");
      } else if (key == ClockwiseParams::getInstance()->PREF_SWAP_BLUE_RED) {
        ClockwiseParams::getInstance()->swapBlueRed = (value == "1");
      } else if (key == ClockwiseParams::getInstance()->PREF_USE_24H_FORMAT) {
        ClockwiseParams::getInstance()->use24hFormat = (value == "1");
      } else if (key == ClockwiseParams::getInstance()->PREF_LDR_PIN) {
        ClockwiseParams::getInstance()->ldrPin = value.toInt();
        pinMode(ClockwiseParams::getInstance()->ldrPin, INPUT);
      } else if (key == ClockwiseParams::getInstance()->PREF_TIME_ZONE) {
        ClockwiseParams::getInstance()->timeZone = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_NTP_SERVER) {
        ClockwiseParams::getInstance()->ntpServer = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_CANVAS_FILE) {
        ClockwiseParams::getInstance()->canvasFile = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_CANVAS_SERVER) {
        ClockwiseParams::getInstance()->canvasServer = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_MANUAL_POSIX) {
        ClockwiseParams::getInstance()->manualPosix = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_DISPLAY_ROTATION) {
        ClockwiseParams::getInstance()->displayRotation = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_DRIVER) {
        ClockwiseParams::getInstance()->driver = value.toInt();
      }  else if (key == ClockwiseParams::getInstance()->PREF_I2CSPEED) {
        ClockwiseParams::getInstance()->i2cSpeed = value.toInt();
      }  else if (key == ClockwiseParams::getInstance()->PREF_E_PIN) {
        ClockwiseParams::getInstance()->E_pin = value.toInt();
      }
      ClockwiseParams::getInstance()->save();
      client.println("HTTP/1.0 204 No Content");
    } else if (method == "GET" && apOn) {
      // Any other GET while setup AP is up → send captive portal page
      // (covers Windows msftconnecttest /redirect and similar).
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html; charset=utf-8");
      client.println("Connection: close");
      client.println("Cache-Control: no-cache");
      client.println();
      client.println(F("<!DOCTYPE html><html><head><meta charset=utf-8>"
                       "<meta http-equiv='refresh' content='0;url=/'>"
                       "<title>Clockwise</title></head><body>"
                       "<p><a href='/'>打开配网页面</a></p>"
                       "<p>或手动输入 <b>192.168.4.1</b></p>"
                       "</body></html>"));
    }
  }

  static String wifiEscape(const String &s) {
    String out;
    out.reserve(s.length());
    for (unsigned i = 0; i < s.length(); i++) {
      char c = s[i];
      if (c == '\\' || c == '|' || c == '\n' || c == '\r') continue;
      out += c;
    }
    return out;
  }

  void writeWifiScan(WiFiClient client) {
    WiFi.mode(WIFI_AP_STA);
    if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
      WiFi.softAP("Clockwise-Wifi", "12345678", 6, false, 4);
      delay(50);
    }
    int n = WiFi.scanNetworks(false, false);
    client.println("HTTP/1.0 200 OK");
    client.println("Content-Type: text/plain; charset=utf-8");
    client.println("Connection: close");
    client.println();
    if (n < 0) {
      client.println("n=0");
      client.println("err=scan");
      return;
    }
    // Deduplicate by SSID, keep strongest.
    struct Hit { String ssid; int32_t rssi; bool secure; int channel; };
    Hit hits[24];
    int hitCount = 0;
    for (int i = 0; i < n && hitCount < 24; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) continue;
      if (ssid == "Clockwise-Wifi") continue;
      int32_t rssi = WiFi.RSSI(i);
      bool secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      int channel = WiFi.channel(i);
      int found = -1;
      for (int j = 0; j < hitCount; j++) {
        if (hits[j].ssid == ssid) { found = j; break; }
      }
      if (found >= 0) {
        if (rssi > hits[found].rssi) {
          hits[found].rssi = rssi;
          hits[found].secure = secure;
          hits[found].channel = channel;
        }
      } else {
        hits[hitCount].ssid = ssid;
        hits[hitCount].rssi = rssi;
        hits[hitCount].secure = secure;
        hits[hitCount].channel = channel;
        hitCount++;
      }
    }
    WiFi.scanDelete();
    // Strongest first.
    for (int i = 0; i < hitCount; i++) {
      for (int j = i + 1; j < hitCount; j++) {
        if (hits[j].rssi > hits[i].rssi) {
          Hit tmp = hits[i];
          hits[i] = hits[j];
          hits[j] = tmp;
        }
      }
    }
    client.printf("n=%d\n", hitCount);
    for (int i = 0; i < hitCount; i++) {
      client.print("net=");
      client.print(wifiEscape(hits[i].ssid));
      client.print('|');
      client.print(hits[i].rssi);
      client.print('|');
      client.print(hits[i].secure ? 1 : 0);
      client.print('|');
      client.println(hits[i].channel);
    }
  }

  void beginStaJoin() {
    // Drop the setup hotspot. Sharing the radio with it stops the clock joining a router.
    WiFi.setSleep(false);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.disconnect(false, true);
    delay(100);
    if (wifiChannel >= 1 && wifiChannel <= 13) {
      WiFi.begin(wifiConnectSsid.c_str(), wifiConnectPwd.c_str(), wifiChannel);
    } else {
      WiFi.begin(wifiConnectSsid.c_str(), wifiConnectPwd.c_str());
    }
    wifiConnectStarted = millis();
    Serial.printf("[WiFi] Joining %s channel %d\n", wifiConnectSsid.c_str(), wifiChannel);
  }

  void restoreSetupAp() {
    WiFi.disconnect(false, false);
    delay(50);
    WiFi.mode(WIFI_AP_STA);
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    bool ok = WiFi.softAP("Clockwise-Wifi", "12345678", 6, false, 4);
    delay(200);
    Serial.printf("[WiFi] Setup AP back %s\n", ok ? "OK" : "FAIL");
  }

  void pollWifiConnect() {
    if (wifiRestartAt != 0 && millis() >= wifiRestartAt) {
      wifiRestartAt = 0;
      force_restart = true;
      return;
    }
    if (wifiArm) {
      wifiArm = false;
      wifiConnecting = true;
      wifiConnectOk = false;
      wifiLastErr = "";
      beginStaJoin();
      return;
    }
    if (!wifiConnecting) return;

    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
      ClockwiseParams *prefs = ClockwiseParams::getInstance();
      prefs->load();
      prefs->wifiSsid = wifiConnectSsid;
      prefs->wifiPwd = wifiConnectPwd;
      prefs->save();
      wifiConnecting = false;
      wifiConnectOk = true;
      wifiLastErr = "";
      wifiRestartAt = millis() + 20000;
      Serial.printf("[WiFi] Verified %s ip=%s\n",
                    wifiConnectSsid.c_str(),
                    WiFi.localIP().toString().c_str());
      return;
    }
    // ESP32 reports CONNECT_FAILED while it is still trying. Wait the full window.
    if ((millis() - wifiConnectStarted) > 25000) {
      wifiLastErr = (st == WL_NO_SSID_AVAIL) ? "not_found" : "timeout";
      wifiConnecting = false;
      wifiConnectOk = false;
      Serial.printf("[WiFi] Connect failed err=%s status=%d\n", wifiLastErr.c_str(), (int)st);
      restoreSetupAp();
    }
  }

  void handleWifi(WiFiClient client, const String &query) {
    ClockwiseParams *prefs = ClockwiseParams::getInstance();
    prefs->load();
    String ssid = queryGet(query, "ssid");
    String forget = queryGet(query, "forget");

    if (forget == "1") {
      prefs->wifiSsid = "";
      prefs->wifiPwd = "";
      prefs->save();
      wifiConnecting = false;
      wifiConnectOk = false;
      wifiLastErr = "";
      wifiArm = false;
      WiFi.disconnect(true, true);
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/plain");
      client.println("Connection: close");
      client.println();
      client.println("ok=1");
      client.println("action=forget");
      client.flush();
      force_restart = true;
      return;
    }

    if (ssid.length() > 0) {
      if (ssid.length() > 32) {
        client.println("HTTP/1.0 400 Bad Request");
        client.println("Content-Type: text/plain");
        client.println("Connection: close");
        client.println();
        client.println("ok=0");
        client.println("err=ssid_long");
        return;
      }
      String pwd = queryGet(query, "pwd");
      if (pwd.length() > 63) pwd = pwd.substring(0, 63);
      int channel = queryGet(query, "ch").toInt();

      // Answer first. The radio switch happens on the next loop, after this reply is sent.
      wifiConnectSsid = ssid;
      wifiConnectPwd = pwd;
      wifiChannel = channel;
      wifiArm = true;
      wifiConnecting = false;
      wifiConnectOk = false;
      wifiLastErr = "";
      wifiRestartAt = 0;

      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/plain; charset=utf-8");
      client.println("Connection: close");
      client.println();
      client.println("ok=pending");
      client.print("ssid=");
      client.println(wifiEscape(ssid));
      client.flush();
      return;
    }

    client.println("HTTP/1.0 200 OK");
    client.println("Content-Type: text/plain; charset=utf-8");
    client.println("Connection: close");
    client.println();
    client.print("ssid=");
    client.println(wifiEscape(wifiConnecting ? wifiConnectSsid : prefs->wifiSsid));
    client.print("sta=");
    client.println(WiFi.status() == WL_CONNECTED ? "1" : "0");
    client.print("connecting=");
    client.println(wifiConnecting ? "1" : "0");
    if (wifiConnectOk || WiFi.status() == WL_CONNECTED) {
      client.println("ok=1");
      client.print("ip=");
      client.println(WiFi.localIP().toString());
    } else if (wifiLastErr.length() > 0 && !wifiConnecting) {
      client.println("ok=0");
      client.print("err=");
      client.println(wifiLastErr);
    } else if (wifiConnecting) {
      client.println("ok=pending");
    } else {
      client.println("ok=0");
    }
  }

  void readPin(WiFiClient client, String key, uint16_t pin) {
    ClockwiseParams::getInstance()->load();
    client.println("HTTP/1.0 204 No Content");
    client.printf(HEADER_TEMPLATE_D, key, analogRead(pin));
    client.println();
  }

  void getCurrentSettings(WiFiClient client) {
    ClockwiseParams::getInstance()->load();
    client.println("HTTP/1.0 204 No Content");
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_DISPLAY_BRIGHT, ClockwiseParams::getInstance()->displayBright);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_DISPLAY_ABC_MIN, ClockwiseParams::getInstance()->autoBrightMin);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_DISPLAY_ABC_MAX, ClockwiseParams::getInstance()->autoBrightMax);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_SWAP_BLUE_GREEN, ClockwiseParams::getInstance()->swapBlueGreen);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_SWAP_BLUE_RED, ClockwiseParams::getInstance()->swapBlueRed);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_USE_24H_FORMAT, ClockwiseParams::getInstance()->use24hFormat);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_LDR_PIN, ClockwiseParams::getInstance()->ldrPin);    
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_TIME_ZONE, ClockwiseParams::getInstance()->timeZone.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_WIFI_SSID, ClockwiseParams::getInstance()->wifiSsid.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_NTP_SERVER, ClockwiseParams::getInstance()->ntpServer.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_CANVAS_FILE, ClockwiseParams::getInstance()->canvasFile.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_CANVAS_SERVER, ClockwiseParams::getInstance()->canvasServer.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_MANUAL_POSIX, ClockwiseParams::getInstance()->manualPosix.c_str());
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_DISPLAY_ROTATION, ClockwiseParams::getInstance()->displayRotation);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_DRIVER, ClockwiseParams::getInstance()->driver);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_I2CSPEED, ClockwiseParams::getInstance()->i2cSpeed);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_E_PIN, ClockwiseParams::getInstance()->E_pin);
    client.printf(HEADER_TEMPLATE_S, "CW_FW_VERSION", CW_FW_VERSION);
    client.printf(HEADER_TEMPLATE_S, "CW_FW_NAME", CW_FW_NAME);
    client.printf(HEADER_TEMPLATE_S, "CLOCKFACE_NAME", CLOCKFACE_NAME);
    client.println();
  }
};
