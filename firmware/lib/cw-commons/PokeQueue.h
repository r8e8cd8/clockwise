#pragma once
#include <Arduino.h>
#include <string.h>

enum PokeAction : uint8_t {
  POKE_NONE = 0,
  POKE_BLINK = 1,
  POKE_SHY = 2,
  POKE_NEXT = 3,
  POKE_SLEEP = 4,
  POKE_SURPRISE = 5,
  POKE_WINK = 6,
  POKE_HEART = 7,
  POKE_SAY = 8,
  POKE_PEEK = 9,
  POKE_GAME_CATCH = 10,
  POKE_GAME_QUIT = 11,
  POKE_GAME_LEFT = 12,
  POKE_GAME_RIGHT = 13,
  POKE_GAME_UP = 14,
  POKE_GAME_DOWN = 15,
  POKE_GAME_SNAKE = 16,
  POKE_GAME_RHYTHM = 17,
  POKE_GAME_START = 10,
  POKE_LOCK = 18,      // msg: none | sleep,mins,intervalSec | shy,10,30
  POKE_DAYNIGHT = 19,  // msg: auto|day|night
  POKE_SECONDS = 20,   // msg: on|off|toggle
  POKE_BYE = 21,       // character slowly slides off; msg: left|right (default right)
  POKE_BACK = 22,      // character slides back
  POKE_FAV = 23,       // msg: toggle|only|all|next
  POKE_CALL = 24,      // knock / call her back from digital mode
  POKE_BG = 25,        // msg: scene index 0..SCENE_COUNT-1
  POKE_MOTTO = 26,     // show Chinese motto on screen
};

struct PokeQueue {
  volatile uint8_t action;
  volatile int8_t stickX;
  volatile int8_t stickY;
  volatile uint8_t laneTap;
  char msg[28];

  static PokeQueue &get() {
    static PokeQueue q;
    return q;
  }

  void request(uint8_t a, const char *m = nullptr) {
    // Directions: set BOTH action and stick so games never miss input
    if (a == POKE_GAME_LEFT) {
      stickX = -1;
      laneTap = 1;
      action = a;
      return;
    }
    if (a == POKE_GAME_DOWN) {
      stickY = 1;
      laneTap = 2;
      action = a;
      return;
    }
    if (a == POKE_GAME_UP) {
      stickY = -1;
      laneTap = 3;
      action = a;
      return;
    }
    if (a == POKE_GAME_RIGHT) {
      stickX = 1;
      laneTap = 4;
      action = a;
      return;
    }
    if (m && m[0]) {
      strncpy(msg, m, sizeof(msg) - 1);
      msg[sizeof(msg) - 1] = '\0';
    } else {
      msg[0] = '\0';
    }
    action = a;
  }

  int8_t takeStickX() { int8_t s = stickX; stickX = 0; return s; }
  int8_t takeStickY() { int8_t s = stickY; stickY = 0; return s; }
  uint8_t takeLane() { uint8_t L = laneTap; laneTap = 0; return L; }
  int8_t takeStick() { return takeStickX(); }

  uint8_t take(char *out = nullptr, size_t outLen = 0) {
    uint8_t a = action;
    if (a == POKE_NONE) return POKE_NONE;
    action = POKE_NONE;
    if (out && outLen) {
      strncpy(out, msg, outLen - 1);
      out[outLen - 1] = '\0';
    }
    return a;
  }
};
