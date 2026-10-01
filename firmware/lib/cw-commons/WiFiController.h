#pragma once

#include "ImprovWiFiLibrary.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include "CWWebServer.h"
#include "StatusController.h"

ImprovWiFi improvSerial(&Serial);

struct WiFiController
{
  long elapsedTimeOffline = 0;
  bool connectionSucessfulOnce = false;
  bool httpOn = false;
  bool setupAp = false;

  static void startMdns()
  {
    MDNS.end();
    if (MDNS.begin("clockwise"))
    {
      MDNS.addService("http", "tcp", 80);
      Serial.println("[WiFi] Open http://clockwise.local/poke");
    }
  }

  static void onImprovWiFiErrorCb(ImprovTypes::Error err)
  {
    ClockwiseWebServer::getInstance()->stopWebServer();
    StatusController::getInstance()->blink_led(2000, 3);
  }

  static void onImprovWiFiConnectedCb(const char *ssid, const char *password)
  {
    ClockwiseParams::getInstance()->load();
    ClockwiseParams::getInstance()->wifiSsid = String(ssid);
    ClockwiseParams::getInstance()->wifiPwd = String(password);
    ClockwiseParams::getInstance()->save();

    ClockwiseWebServer::getInstance()->startWebServer();
    startMdns();
  }

  bool isConnected()
  {
    if (improvSerial.isConnected()) {
      elapsedTimeOffline = 0;
      return true;
    }
    if (elapsedTimeOffline == 0 && !connectionSucessfulOnce)
      elapsedTimeOffline = millis();
    return false;
  }

  static void handleImprovWiFi()
  {
    improvSerial.handleSerial();
  }

  void processDns()
  {
    // WiFiManager owns DNS while its portal is open (blocking).
  }

  bool servesHttp()
  {
    return httpOn;
  }

  void clearSavedWifi()
  {
    ClockwiseParams *prefs = ClockwiseParams::getInstance();
    prefs->load();
    prefs->wifiSsid = "";
    prefs->wifiPwd = "";
    prefs->save();
    WiFi.disconnect(true, true);
    delay(100);
  }

  // Official Clockwise SoftAP page: list of networks + password (WiFiManager).
  bool alternativeSetupMethod()
  {
    StatusController::getInstance()->wifiConnectionFailed("Setup WiFi via AP");

    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(300); // 5 min
    wifiManager.setBreakAfterConfig(true);
    wifiManager.setHostname("clockwise");

    Serial.println("[WiFi] WiFiManager portal: Clockwise-Wifi / 12345678");
    Serial.println("[WiFi] Phone → connect AP → open http://192.168.4.1/ → Configure WiFi");

    // Same as official docs: hotspot name + password, selectable network list in browser.
    bool success = wifiManager.startConfigPortal("Clockwise-Wifi", "12345678");

    if (success)
    {
      onImprovWiFiConnectedCb(WiFi.SSID().c_str(), WiFi.psk().c_str());
      connectionSucessfulOnce = true;
      httpOn = true;
      setupAp = false;
      Serial.printf("[WiFi] Connected via WiFiManager to %s, IP %s\n",
                    WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      return true;
    }

    Serial.println("[WiFi] WiFiManager portal timed out / failed");
    return false;
  }

  bool trySavedNetwork(uint16_t attempts)
  {
    ClockwiseParams::getInstance()->load();
    String ssid = ClockwiseParams::getInstance()->wifiSsid;
    String pwd = ClockwiseParams::getInstance()->wifiPwd;
    if (ssid.isEmpty())
      return false;

    Serial.printf("[WiFi] Trying saved SSID %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(ssid.c_str(), pwd.c_str());
    for (uint16_t i = 0; i < attempts; i++)
    {
      if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0))
      {
        connectionSucessfulOnce = true;
        httpOn = true;
        setupAp = false;
        ClockwiseWebServer::getInstance()->startWebServer();
        startMdns();
        Serial.printf("[WiFi] Connected to %s, IP %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        return true;
      }
      delay(250);
      yield();
    }
    WiFi.disconnect(false, false);
    return false;
  }

  bool begin()
  {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);

    improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32, CW_FW_NAME, CW_FW_VERSION, "Clockwise");
    improvSerial.onImprovError(onImprovWiFiErrorCb);
    improvSerial.onImprovConnected(onImprovWiFiConnectedCb);

    // One-shot after this flash: clear old creds and open official WiFiManager page.
    {
      Preferences nvs;
      if (nvs.begin("clockwise", false)) {
        if (!nvs.getBool("wmPortal1", false)) {
          nvs.putBool("wmPortal1", true);
          nvs.end();
          clearSavedWifi();
          Serial.println("[WiFi] wmPortal1: forcing WiFiManager setup");
          return alternativeSetupMethod();
        }
        nvs.end();
      }
    }

    if (trySavedNetwork(32))
      return true;

    clearSavedWifi();
    return alternativeSetupMethod();
  }
};
