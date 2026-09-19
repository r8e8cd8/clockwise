#pragma once
#include <Arduino.h>
#include <WiFi.h>

// Lets the phone page read and switch portrait backgrounds without
// pulling the scene tables into the web server.
struct SceneBridge {
  static uint8_t current();
  static uint8_t count();
  static void select(uint8_t idx);
  static void writeBmp(WiFiClient &client, uint8_t idx);
  static void writeScreen(WiFiClient &client);
};
