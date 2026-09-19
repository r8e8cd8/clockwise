#pragma once

#include <Adafruit_GFX.h>
#include <string.h>

// Forwards drawing to the LED panel and keeps a 64x64 RGB565 copy for /screen.
class MirrorGFX : public Adafruit_GFX {
 public:
  static MirrorGFX *instance;

  explicit MirrorGFX(Adafruit_GFX *dst) : Adafruit_GFX(64, 64), _dst(dst) {
    instance = this;
    memset(_snap, 0, sizeof(_snap));
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if ((uint16_t)x < 64 && (uint16_t)y < 64) {
      _snap[(uint16_t)y * 64 + (uint16_t)x] = color;
    }
    if (_dst) _dst->drawPixel(x, y, color);
  }

  void fillScreen(uint16_t color) override {
    for (int i = 0; i < 64 * 64; i++) _snap[i] = color;
    if (_dst) _dst->fillScreen(color);
  }

  const uint16_t *pixels() const { return _snap; }

 private:
  Adafruit_GFX *_dst;
  uint16_t _snap[64 * 64];
};
