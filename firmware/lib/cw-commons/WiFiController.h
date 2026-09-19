#pragma once

#include "ImprovWiFiLibrary.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include "CWWebServer.h"
#include "StatusController.h"
#include <WiFiManager.h>

ImprovWiFi improvSerial(&Serial);

struct WiFiController
{
  long elapsedTimeOffline = 0;
  bool connectionSucessfulOnce;

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
    } else {
      if (elapsedTimeOffline == 0 && !connectionSucessfulOnce)
        elapsedTimeOffline = millis();
      
      if ((millis() - elapsedTimeOffline) > 1000 * 60 * 5)  // restart if clockface is not showed and is 5min offline 
        StatusController::getInstance()->forceRestart();

      return false;
    }
  }

  static void handleImprovWiFi()
  {
    improvSerial.handleSerial();
  }

  bool alternativeSetupMethod()
  {
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(300); //Wait 5min to configure wifi via AP

    bool success = wifiManager.startConfigPortal("Clockwise-Wifi");

    if (success)
    {
      onImprovWiFiConnectedCb(WiFi.SSID().c_str(), WiFi.psk().c_str());
      Serial.printf("[WiFi] Connected via WiFiManager to %s, IP address %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      connectionSucessfulOnce = success;
    }

    return success;
  }

  // Home LAN static IP (was previously assigned by DHCP as .93)
  static void applyStaticIp()
  {
    IPAddress local(192, 168, 3, 93);
    IPAddress gateway(192, 168, 3, 1);
    IPAddress subnet(255, 255, 255, 0);
    IPAddress dns(192, 168, 3, 1);
    if (!WiFi.config(local, gateway, subnet, dns))
      Serial.println("[WiFi] Static IP config failed");
    else
      Serial.println("[WiFi] Static IP 192.168.3.93");
  }

  bool begin()
  {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    applyStaticIp();

    improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32, CW_FW_NAME, CW_FW_VERSION, "Clockwise");
    improvSerial.onImprovError(onImprovWiFiErrorCb);
    improvSerial.onImprovConnected(onImprovWiFiConnectedCb);

    ClockwiseParams::getInstance()->load();

    if (!ClockwiseParams::getInstance()->wifiSsid.isEmpty())
    {
      if (improvSerial.tryConnectToWifi(ClockwiseParams::getInstance()->wifiSsid.c_str(), ClockwiseParams::getInstance()->wifiPwd.c_str()))
      {
        connectionSucessfulOnce = true;
        ClockwiseWebServer::getInstance()->startWebServer();
        startMdns();
        Serial.printf("[WiFi] Connected to %s, IP address %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        Serial.println("[WiFi] Open http://192.168.3.93/poke");
        return true;
      }
    }      

    StatusController::getInstance()->wifiConnectionFailed("Setup WiFi via AP");
    alternativeSetupMethod();

    StatusController::getInstance()->wifiConnectionFailed("WiFi Failed");
    return false;
  }
};
