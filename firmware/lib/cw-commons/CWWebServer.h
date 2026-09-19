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
            uint8_t method_pos = httpBuffer.indexOf(' ');
            uint8_t path_pos = httpBuffer.indexOf(' ', method_pos + 1);

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
    if (method == "GET" && path == "/") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      client.println(SETTINGS_PAGE);
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
