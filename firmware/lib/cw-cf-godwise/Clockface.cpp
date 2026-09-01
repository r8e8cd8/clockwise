#include "Clockface.h"
#include <PokeQueue.h>
#include <CWPreferences.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

static const uint16_t COL_FACE = 0x18C3;
static const uint16_t COL_RIM = 0xBC8A;
static const uint16_t COL_HAND_H = 0xEED6;
static const uint16_t COL_HAND_M = 0xBC8A;
static const uint16_t COL_HAND_S = 0xF81F;
static const uint16_t COL_TICK = 0x9C6D;
static const uint16_t COL_BUBBLE = 0xFFFF;
static const uint16_t COL_BUBBLE_BG = 0x3186;
static const uint16_t COL_HEART = 0xF80F;
static const uint16_t COL_Z = 0xC618;
static const uint16_t COL_SPARK = 0xFFE0;
static const uint16_t COL_CYAN = 0x07FF;

static const uint16_t BLINK_DUR[3] = {55, 110, 55};
static const int DIAL_CX = 12;
static const int DIAL_CY = 12;
static const int DIAL_R = 10;
static const int DIAL_CLEAR = DIAL_R + 2;

// Tiny 5x7 A-Z 0-9 for English bubble
static const uint8_t FONT5X7[][7] PROGMEM = {
  {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
  {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
  {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
  {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
  {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
  {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
  {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
  {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
  {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
  {0x01,0x01,0x01,0x01,0x11,0x11,0x0E},
  {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
  {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
  {0x11,0x1B,0x15,0x11,0x11,0x11,0x11},
  {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
  {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
  {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
  {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
  {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
  {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
  {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
  {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
  {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
  {0x11,0x11,0x11,0x15,0x15,0x15,0x0A},
  {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
  {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
  {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
  {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
  {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
  {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},
  {0x1F,0x01,0x02,0x06,0x01,0x11,0x0E},
  {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
  {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
  {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
  {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
  {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
  {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
  {0x00,0x04,0x04,0x00,0x04,0x04,0x00},
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},
};

static int fontIndex(char ch) {
  if (ch >= 'A' && ch <= 'Z') return ch - 'A';
  if (ch >= 'a' && ch <= 'z') return ch - 'a';
  if (ch >= '0' && ch <= '9') return 26 + (ch - '0');
  if (ch == '!') return 36;
  if (ch == ' ') return 37;
  if (ch == '.' || ch == ',') return 38;
  return 37;
}

Clockface::Clockface(Adafruit_GFX* display) {
  _display = display;
  _dateTime = nullptr;
  _lastAnimMs = 0;
  _nextBlinkMs = 0;
  _blinkStepMs = 0;
  _nextSceneMs = 0;
  _effectUntilMs = 0;
  _bubbleUntilMs = 0;
  _fxTickMs = 0;
  _blinkFrame = -1;
  _lastHour = -1;
  _lastMinute = -1;
  _blushLevel = 1;
  _sceneIdx = 0;
  _poolIdx = 0;
  _wasDay = true;
  _effect = 0;
  _bubble[0] = 0;
  _bubbleOn = false;
  _moodLock = 0;
  _moodLockUntilMs = 0;
  _moodLockNextMs = 0;
  _moodLockIntervalMs = 30000;
  _dayMode = 0;
  _showSeconds = false;
  _lastSecond = -1;
  _charOx = 0;
  _charOy = 0;
  _slideMode = 0;
  _slideTickMs = 0;
  _dialAnimR = DIAL_R;
  _dialAnimRot = 0;
  _digitalColon = true;
  _gameTickMs = 0;
  _gameMode = GM_OFF;
  _gameOver = false;
  _paddleX = 28;
  _score = 0;
  _miss = 0;
  _snLen = 0;
  _snDir = 1;
  _snPend = 1;
  _foodX = 8;
  _foodY = 8;
  _rhChartI = 0;
  _rhNextBeat = 0;
  _combo = 0;
  memset(_gx, 0, sizeof(_gx));
  memset(_gy, 0, sizeof(_gy));
  memset(_gs, 0, sizeof(_gs));
  memset(_snX, 0, sizeof(_snX));
  memset(_snY, 0, sizeof(_snY));
  memset(_rhLane, -1, sizeof(_rhLane));
  memset(_rhY, 0, sizeof(_rhY));
  memset(_fx, 0, sizeof(_fx));
  memset(_fy, 0, sizeof(_fy));
  Locator::provide(display);
}

bool Clockface::isDayHour(int hour) const {
  if (_dayMode == 1) return true;
  if (_dayMode == 2) return false;
  return hour >= DAY_HOUR_START && hour < DAY_HOUR_END;
}

void Clockface::syncSceneToHour(bool forceRedraw) {
  if (!_dateTime) return;
  bool day = isDayHour(_dateTime->getHour());
  uint8_t count = day ? DAY_SCENE_COUNT : NIGHT_SCENE_COUNT;
  uint8_t base = day ? 0 : DAY_SCENE_COUNT;
  if (count == 0) return;
  if (day != _wasDay) {
    _poolIdx = 0;
    _wasDay = day;
    forceRedraw = true;
  }
  if (_poolIdx >= count) _poolIdx = 0;
  uint8_t next = base + _poolIdx;
  if (forceRedraw || next != _sceneIdx) {
    _sceneIdx = next;
    _blinkFrame = -1;
    if (_effect == 0) {
      _bubbleOn = false;
    }
    drawFullFrame();
  }
}

void Clockface::advanceScene() {
  if (!_dateTime) return;
  bool day = isDayHour(_dateTime->getHour());
  uint8_t count = day ? DAY_SCENE_COUNT : NIGHT_SCENE_COUNT;
  uint8_t base = day ? 0 : DAY_SCENE_COUNT;
  if (count == 0) return;
  _poolIdx = (_poolIdx + 1) % count;
  _sceneIdx = base + _poolIdx;
  _wasDay = day;
  _blinkFrame = -1;
  if (_effect == 0) {
    _bubbleOn = false;
  }
  drawFullFrame();
  _nextSceneMs = millis() + SCENE_MS;
}

void Clockface::drawFullFrame() {
  if (_slideMode == 3) {
    drawDigitalClock();
    return;
  }
  if (_slideMode == 2) {
    int8_t savedOx = _charOx;
    _charOx = 80;
    drawPortraitRegion(0, 0, 64, 64, false);
    _charOx = savedOx;
    drawDialMorph();
    return;
  }

  if (_slideMode == 1 || _slideMode == 4) {
    // Skip dial box so the corner clock is never wiped/redrawn (no flicker)
    drawPortraitRegion(0, 0, 64, 64, true);
    return;
  }

  drawPortraitRegion(0, 0, 64, 64, false);
  applyBlinkFrame(_blinkFrame);
  drawBlush(_blushLevel);
  drawTimeAnalog(false);
  if (_bubbleOn) drawBubble();
}

void Clockface::setup(CWDateTime* dateTime) {
  _dateTime = dateTime;
  Locator::getDisplay()->fillScreen(BG_COLOR);
  _poolIdx = 0;
  _wasDay = isDayHour(_dateTime->getHour());
  _sceneIdx = _wasDay ? 0 : DAY_SCENE_COUNT;
  drawFullFrame();
  _lastHour = _dateTime->getHour();
  _lastMinute = _dateTime->getMinute();
  unsigned long now = millis();
  _lastAnimMs = now;
  _nextBlinkMs = now + 2800;
  _nextSceneMs = now + SCENE_MS;
  _blinkFrame = -1;
}

void Clockface::clearMoodLock(unsigned long now) {
  _moodLock = 0;
  _moodLockUntilMs = 0;
  _moodLockNextMs = 0;
  if (_effect != 0) {
    clearEffectArt();
    restoreEyes();
    _blushLevel = 1;
    drawBlush(1);
    _effect = 0;
  }
  _effectUntilMs = now;
}

void Clockface::triggerMoodBurst(unsigned long now) {
  if (!_moodLock) return;
  _blinkFrame = -1;
  if (_moodLock == POKE_SLEEP) {
    _effect = POKE_SLEEP;
    _effectUntilMs = now + 3500;
    applySleepClosed();
    spawnSleepFx();
    _fxTickMs = now;
  } else if (_moodLock == POKE_SHY) {
    _effect = POKE_SHY;
    _effectUntilMs = now + 3200;
    _blushLevel = 2;
    drawBlush(2);
    drawHeart(18, 30, COL_HEART);
    drawHeart(44, 30, COL_HEART);
  } else if (_moodLock == POKE_HEART) {
    _effect = POKE_HEART;
    _effectUntilMs = now + 2800;
    _blushLevel = 2;
    drawBlush(2);
    spawnHeartFx();
    _fxTickMs = now;
  } else if (_moodLock == POKE_PEEK) {
    _effect = POKE_PEEK;
    _effectUntilMs = now + 1600;
    applyPeekEyes();
  }
}

void Clockface::setMoodLock(uint8_t mode, unsigned long now, uint16_t minutes, uint16_t intervalSec) {
  if (mode != POKE_SLEEP && mode != POKE_SHY && mode != POKE_HEART && mode != POKE_PEEK) {
    clearMoodLock(now);
    return;
  }
  if (minutes < 1) minutes = 1;
  if (minutes > 180) minutes = 180;
  if (intervalSec < 5) intervalSec = 5;
  if (intervalSec > 300) intervalSec = 300;
  _moodLock = mode;
  _moodLockUntilMs = now + (unsigned long)minutes * 60000UL;
  _moodLockIntervalMs = (unsigned long)intervalSec * 1000UL;
  triggerMoodBurst(now);
  _moodLockNextMs = now + _moodLockIntervalMs;
}

void Clockface::setDayMode(uint8_t mode) {
  if (mode > 2) mode = 0;
  _dayMode = mode;
  _poolIdx = 0;
  syncSceneToHour(true);
}

void Clockface::setShowSeconds(bool on) {
  _showSeconds = on;
  _lastSecond = -1;
  drawTimeAnalog();
}

bool Clockface::charVisible() const {
  return _slideMode != 2 && _slideMode != 3 && _charOx < 56 && _charOx > -56;
}

bool Clockface::inPortraitUi() const {
  return _slideMode == 0;
}

void Clockface::startSlideLeave() {
  if (_slideMode == 1 || _slideMode == 2 || _slideMode == 3) return;
  if (_slideMode == 4) {
    _slideMode = 1;
    _slideTickMs = millis();
    return;
  }
  _bubbleOn = false;
  if (_effect != 0) {
    clearEffectArt();
    _effect = 0;
  }
  _blinkFrame = -1;
  // paint dial once, then freeze it while character walks
  drawPortraitRegion(0, 0, 64, 64, false);
  drawTimeAnalog(false);
  _slideMode = 1;
  _slideTickMs = millis();
}

void Clockface::startSlideBack() {
  if (_slideMode == 0) return;
  if (_slideMode == 1) {
    _slideMode = 4;
    _slideTickMs = millis();
    return;
  }
  _slideMode = 4;
  _charOx = 64;
  _charOy = 6;
  _dialAnimR = DIAL_R;
  _dialAnimRot = 0;
  _slideTickMs = millis();
  // bg + dial once, then freeze dial while walking back
  int8_t saved = _charOx;
  drawPortraitRegion(0, 0, 64, 64, false);
  _charOx = saved;
  drawTimeAnalog(false);
}

void Clockface::beginDialVanish() {
  _slideMode = 2;
  _charOx = 64;
  _charOy = 8;
  _dialAnimR = DIAL_R;
  _dialAnimRot = 0;
  _slideTickMs = millis();
  drawFullFrame();
}

void Clockface::enterDigitalMode() {
  _slideMode = 3;
  _charOx = 64;
  _digitalColon = true;
  _lastHour = -1;  // force digital redraw
  _lastMinute = -1;
  _lastSecond = -1;
  drawDigitalClock();
}

void Clockface::tickSlide(unsigned long now) {
  if (_slideMode == 1 || _slideMode == 4) {
    // ~3s across: 1px / 47ms
    if (now - _slideTickMs < 47) return;
    _slideTickMs = now;

    if (_slideMode == 1) {
      _charOx = (int8_t)(_charOx + 1);
      if ((_charOx & 3) == 0 && _charOy < 6) _charOy++;
      if (_charOx >= 64) {
        _charOx = 64;
        beginDialVanish();
        return;
      }
    } else {
      _charOx = (int8_t)(_charOx - 1);
      if (_charOy > 0 && (_charOx & 3) == 0) _charOy--;
      if (_charOx <= 0) {
        _charOx = 0;
        _charOy = 0;
        _slideMode = 0;
        _dialAnimR = DIAL_R;
        _dialAnimRot = 0;
        drawFullFrame();
        return;
      }
    }
    drawFullFrame();
    return;
  }

  if (_slideMode == 2) {
    if (now - _slideTickMs < 40) return;
    _slideTickMs = now;
    _dialAnimRot = (int16_t)(_dialAnimRot + 22);
    if (_dialAnimR > 0) _dialAnimR--;
    if (_dialAnimR <= 0) {
      enterDigitalMode();
      return;
    }
    drawFullFrame();
    return;
  }

  if (_slideMode == 3) {
    // colon blink + time refresh handled in tickAnimation
  }
}

void Clockface::update() {
  if (!_dateTime) return;
  unsigned long now = millis();
  handlePoke(now);
  if (_gameMode != GM_OFF) {
    gameTick(now);
    return;
  }
  tickAnimation(now);
}

void Clockface::externalEvent(int type) {
  (void)type;
  drawFullFrame();
}

void Clockface::handlePoke(unsigned long now) {
  char msg[28];
  uint8_t a = PokeQueue::get().take(msg, sizeof(msg));

  if (a == POKE_GAME_CATCH || a == POKE_GAME_START) {
    gameStart(GM_CATCH);
    return;
  }
  if (a == POKE_GAME_SNAKE) {
    gameStart(GM_SNAKE);
    return;
  }
  if (a == POKE_GAME_RHYTHM) {
    gameStart(GM_RHYTHM);
    return;
  }
  if (a == POKE_GAME_QUIT) {
    gameQuit();
    return;
  }

  // In-game controls must be handled here (not ignored)
  if (_gameMode == GM_SNAKE && !_gameOver) {
    if (a == POKE_GAME_LEFT && _snDir != 1) _snPend = 0;
    else if (a == POKE_GAME_RIGHT && _snDir != 0) _snPend = 1;
    else if (a == POKE_GAME_UP && _snDir != 3) _snPend = 2;
    else if (a == POKE_GAME_DOWN && _snDir != 2) _snPend = 3;
    return;
  }
  if (_gameMode == GM_CATCH && !_gameOver) {
    if (a == POKE_GAME_LEFT) {
      _paddleX -= 4;
      if (_paddleX < 1) _paddleX = 1;
    } else if (a == POKE_GAME_RIGHT) {
      _paddleX += 4;
      if (_paddleX > 64 - 12 - 1) _paddleX = 64 - 12 - 1;
    }
    return;
  }
  if (_gameMode == GM_RHYTHM && !_gameOver) {
    // lane also arrives via takeLane in rhythmTick; action is enough as backup
    if (a >= POKE_GAME_LEFT && a <= POKE_GAME_DOWN) {
      uint8_t lane = 0;
      if (a == POKE_GAME_LEFT) lane = 1;
      else if (a == POKE_GAME_DOWN) lane = 2;
      else if (a == POKE_GAME_UP) lane = 3;
      else lane = 4;
      // stash into laneTap if empty
      if (PokeQueue::get().laneTap == 0) PokeQueue::get().laneTap = lane;
    }
    return;
  }
  if (_gameMode != GM_OFF) return;
  if (a == POKE_NONE) return;

  if (a == POKE_LOCK) {
    // msg: "none" | "sleep,10,30" = mode, durationMinutes, intervalSeconds
    uint8_t mode = 0;
    uint16_t minutes = 10;
    uint16_t intervalSec = 30;
    char buf[28];
    strncpy(buf, msg, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    char *p1 = strchr(buf, ',');
    char *p2 = nullptr;
    if (p1) {
      *p1 = 0;
      p2 = strchr(p1 + 1, ',');
      if (p2) {
        *p2 = 0;
        int m = atoi(p1 + 1);
        int iv = atoi(p2 + 1);
        if (m > 0) minutes = (uint16_t)m;
        if (iv > 0) intervalSec = (uint16_t)iv;
      } else {
        int m = atoi(p1 + 1);
        if (m > 0) minutes = (uint16_t)m;
      }
    }
    if (!strcmp(buf, "sleep")) mode = POKE_SLEEP;
    else if (!strcmp(buf, "shy")) mode = POKE_SHY;
    else if (!strcmp(buf, "heart")) mode = POKE_HEART;
    else if (!strcmp(buf, "peek")) mode = POKE_PEEK;
    else mode = 0;
    setMoodLock(mode, now, minutes, intervalSec);
    return;
  }
  if (a == POKE_DAYNIGHT) {
    uint8_t dm = 0;
    if (!strcmp(msg, "day")) dm = 1;
    else if (!strcmp(msg, "night")) dm = 2;
    else dm = 0;
    setDayMode(dm);
    return;
  }
  if (a == POKE_SECONDS) {
    if (!strcmp(msg, "on")) setShowSeconds(true);
    else if (!strcmp(msg, "off")) setShowSeconds(false);
    else setShowSeconds(!_showSeconds);
    return;
  }
  if (a == POKE_BYE) {
    startSlideLeave();
    return;
  }
  if (a == POKE_BACK) {
    startSlideBack();
    return;
  }

  // During schedule window still allow next / say / lock / day / sec / slide
  if (_moodLock && a != POKE_NEXT && a != POKE_SAY) {
    return;
  }

  if (a == POKE_BLINK) {
    _blinkFrame = 0;
    _blinkStepMs = now + BLINK_DUR[0];
    applyBlinkFrame(0);
  } else if (a == POKE_WINK) {
    _effect = POKE_WINK;
    _effectUntilMs = now + 220;
    applyWinkClosed();
  } else if (a == POKE_PEEK) {
    _effect = POKE_PEEK;
    _effectUntilMs = now + 1600;
    applyPeekEyes();
  } else if (a == POKE_SHY) {
    _effect = POKE_SHY;
    _effectUntilMs = now + 3200;
    _blushLevel = 2;
    drawBlush(2);
    drawHeart(18, 30, COL_HEART);
    drawHeart(44, 30, COL_HEART);
  } else if (a == POKE_NEXT) {
    advanceScene();
  } else if (a == POKE_SLEEP) {
    _effect = POKE_SLEEP;
    _effectUntilMs = now + 3500;
    _blinkFrame = -1;
    applySleepClosed();
    spawnSleepFx();
    _fxTickMs = now;
  } else if (a == POKE_HEART) {
    _effect = POKE_HEART;
    _effectUntilMs = now + 2800;
    _blushLevel = 2;
    drawBlush(2);
    spawnHeartFx();
    _fxTickMs = now;
  } else if (a == POKE_SURPRISE) {
    _effect = POKE_SURPRISE;
    _effectUntilMs = now + 2500;
    _blushLevel = 2;
    drawBlush(2);
    spawnSurpriseFx();
    _fxTickMs = now;
  } else if (a == POKE_SAY) {
    if (msg[0] == 0) strncpy(msg, "Hi!", sizeof(msg) - 1);
    for (char *p = msg; *p; ++p) {
      if ((unsigned char)*p > 127) *p = ' ';
    }
    strncpy(_bubble, msg, sizeof(_bubble) - 1);
    _bubble[sizeof(_bubble) - 1] = 0;
    _bubbleOn = true;
    _bubbleUntilMs = now + 4000;
    drawBubble();
  }
}

void Clockface::tickAnimation(unsigned long now) {
  int h = _dateTime->getHour();
  int m = _dateTime->getMinute();
  int s = _dateTime->getSecond();

  // Schedule window: auto-fire bursts at interval; between bursts act normal
  if (_moodLock && _slideMode == 0) {
    if (_moodLockUntilMs != 0 && (long)(now - _moodLockUntilMs) >= 0) {
      clearMoodLock(now);
    } else if (now >= _moodLockNextMs) {
      triggerMoodBurst(now);
      _moodLockNextMs = now + _moodLockIntervalMs;
    }
  }

  if (h != _lastHour && _slideMode == 0) {
    syncSceneToHour(true);
  }

  if (now >= _nextSceneMs) {
    if (_slideMode == 0) {
      advanceScene();
      _lastHour = h;
      _lastMinute = m;
      _lastSecond = s;
    } else {
      _nextSceneMs = now + SCENE_MS;
    }
    return;
  }

  tickSlide(now);

  if (_slideMode == 3) {
    bool minuteChanged = (h != _lastHour || m != _lastMinute);
    bool timeChanged = minuteChanged || (s != _lastSecond);
    // colon blink ~1.2s period
    if (timeChanged || (now - _slideTickMs >= 1200)) {
      if (now - _slideTickMs >= 1200) {
        _digitalColon = !_digitalColon;
        _slideTickMs = now;
      }
      // hourglass finished a minute → next background
      if (minuteChanged && _lastMinute >= 0) {
        bool day = isDayHour(h);
        uint8_t count = day ? DAY_SCENE_COUNT : NIGHT_SCENE_COUNT;
        uint8_t base = day ? 0 : DAY_SCENE_COUNT;
        if (count > 0) {
          _poolIdx = (_poolIdx + 1) % count;
          _sceneIdx = base + _poolIdx;
          _wasDay = day;
        }
      }
      _lastHour = h;
      _lastMinute = m;
      _lastSecond = s;
      drawDigitalClock();
    }
    return;
  }

  // During leave / dial morph / return: no separate dial time refresh (stops flicker)
  if (_slideMode != 0) return;

  bool timeChanged = (h != _lastHour || m != _lastMinute ||
                      (_showSeconds && s != _lastSecond));
  if (timeChanged) {
    _lastHour = h;
    _lastMinute = m;
    _lastSecond = s;
    drawTimeAnalog(true);
    if (_bubbleOn) drawBubble();
  }

  bool pauseBlink = (_effect == POKE_SLEEP || _effect == POKE_WINK || _effect == POKE_PEEK);
  if (!pauseBlink) {
    if (_blinkFrame < 0 && now >= _nextBlinkMs) {
      _blinkFrame = 0;
      _blinkStepMs = now + BLINK_DUR[0];
      applyBlinkFrame(0);
    } else if (_blinkFrame >= 0 && now >= _blinkStepMs) {
      _blinkFrame++;
      if (_blinkFrame >= 3) {
        _blinkFrame = -1;
        restoreEyes();
        drawBlush(_blushLevel);
        _nextBlinkMs = now + 2800 + (now % 2200);
        if ((now / 17) % 5 == 0) _nextBlinkMs = now + 280;
      } else {
        _blinkStepMs = now + BLINK_DUR[_blinkFrame];
        applyBlinkFrame(_blinkFrame);
      }
    }
  }

  if (now - _lastAnimMs >= 8000 + (now % 4000) && _blinkFrame < 0 &&
      _effect != POKE_SHY && _effect != POKE_SLEEP) {
    _blushLevel = (_blushLevel % 2) + 1;
    drawPortraitRegion(BLUSH_LX - 2, BLUSH_Y - 1, 5, 4);
    drawPortraitRegion(BLUSH_RX - 2, BLUSH_Y - 1, 5, 4);
    drawBlush(_blushLevel);
    _lastAnimMs = now;
  }

  if (_bubbleOn && now >= _bubbleUntilMs) {
    _bubbleOn = false;
    clearBubbleArea();
    drawTimeAnalog(true);
  }

  if (_effect != 0 && now >= _effectUntilMs) {
    clearEffectArt();
    if (_effect == POKE_SLEEP || _effect == POKE_WINK || _effect == POKE_PEEK) {
      restoreEyes();
      drawBlush(_blushLevel);
    }
    if (_bubbleOn) drawBubble();
    _effect = 0;
    _blushLevel = 1;
  }

  if ((_effect == POKE_SURPRISE || _effect == POKE_SLEEP || _effect == POKE_HEART) &&
      now - _fxTickMs >= 70) {
    tickFxParticles();
    _fxTickMs = now;
  }
}

void Clockface::clearBubbleArea() {
  drawPortraitRegion(0, 52, 64, 12);
}

void Clockface::clearEffectArt() {
  // Full mid-face wipe so hearts / sparks leave no trails
  drawPortraitRegion(0, 8, 64, 44);
  for (uint8_t i = 0; i < FX_N; i++) {
    _fx[i] = -1;
    _fy[i] = -1;
  }
  drawBlush(_blushLevel);
  drawTimeAnalog();
  if (_bubbleOn) drawBubble();
}

void Clockface::drawPortraitRegion(int x, int y, int w, int h) {
  drawPortraitRegion(x, y, w, h, false);
}

void Clockface::drawPortraitRegion(int x, int y, int w, int h, bool skipDial) {
  Adafruit_GFX* d = Locator::getDisplay();
  uint8_t s = _sceneIdx;
  if (s >= SCENE_COUNT) s = 0;
  const int dx0 = DIAL_CX - DIAL_CLEAR;
  const int dy0 = DIAL_CY - DIAL_CLEAR;
  const int dx1 = dx0 + DIAL_CLEAR * 2;
  const int dy1 = dy0 + DIAL_CLEAR * 2;
  for (int yy = y; yy < y + h; yy++) {
    if (yy < 0 || yy >= 64) continue;
    for (int xx = x; xx < x + w; xx++) {
      if (xx < 0 || xx >= 64) continue;
      if (skipDial && xx >= dx0 && xx <= dx1 && yy >= dy0 && yy <= dy1) continue;
      uint16_t color = pgm_read_word(&PORTRAIT_SCENES[s][yy * 64 + xx]);
#if defined(HAS_CHAR_LAYER) && HAS_CHAR_LAYER
      int cx = xx - _charOx;
      int cy = yy - _charOy;
      if (cx >= 0 && cx < 64 && cy >= 0 && cy < 64) {
        uint16_t idx = (uint16_t)cy * 64 + (uint16_t)cx;
        uint8_t mb = pgm_read_byte(&CHAR_MASK[idx >> 3]);
        if (mb & (1 << (idx & 7))) {
          color = pgm_read_word(&CHAR_RGB[idx]);
        }
      }
#endif
      d->drawPixel(xx, yy, color);
    }
  }
}

void Clockface::drawPatch(int eyeCx, int eyeCy, const uint16_t* patch) {
  Adafruit_GFX* d = Locator::getDisplay();
  int x0 = eyeCx + PATCH_OX + _charOx;
  int y0 = eyeCy + PATCH_OY + _charOy;
  for (int y = 0; y < PATCH_H; y++) {
    for (int x = 0; x < PATCH_W; x++) {
      int xx = x0 + x;
      int yy = y0 + y;
      if (xx < 0 || xx >= 64 || yy < 0 || yy >= 64) continue;
      d->drawPixel(xx, yy, pgm_read_word(&patch[y * PATCH_W + x]));
    }
  }
}

void Clockface::applyBlinkFrame(int8_t frame) {
  if (frame < 0) return;
  const uint16_t* left = (frame == 1) ? EYE_CLOSED_L : EYE_HALF_L;
  const uint16_t* right = (frame == 1) ? EYE_CLOSED_R : EYE_HALF_R;
  drawPatch(EYE_LX, EYE_LY, left);
  drawPatch(EYE_RX, EYE_RY, right);
}

void Clockface::applyWinkClosed() {
  drawPatch(EYE_RX, EYE_RY, EYE_CLOSED_R);
}

void Clockface::applySleepClosed() {
  drawPatch(EYE_LX, EYE_LY, EYE_CLOSED_L);
  drawPatch(EYE_RX, EYE_RY, EYE_CLOSED_R);
}

void Clockface::applyPeekEyes() {
  // half-open "peeking" look
  drawPatch(EYE_LX, EYE_LY, EYE_HALF_L);
  drawPatch(EYE_RX, EYE_RY, EYE_HALF_R);
}

void Clockface::restoreEyes() {
  drawPortraitRegion(EYE_LX + PATCH_OX + _charOx, EYE_LY + PATCH_OY + _charOy, PATCH_W, PATCH_H);
  drawPortraitRegion(EYE_RX + PATCH_OX + _charOx, EYE_RY + PATCH_OY + _charOy, PATCH_W, PATCH_H);
}

static void drawHand(Adafruit_GFX* d, int cx, int cy, float deg, int len, uint16_t color) {
  float rad = deg * (float)M_PI / 180.0f;
  int x2 = cx + (int)(len * cosf(rad) + 0.5f);
  int y2 = cy + (int)(len * sinf(rad) + 0.5f);
  d->drawLine(cx, cy, x2, y2, color);
}

void Clockface::drawTimeAnalog(bool clearBg) {
  if (!_dateTime) return;
  if (_slideMode == 2 || _slideMode == 3) return;
  Adafruit_GFX* d = Locator::getDisplay();
  int x0 = DIAL_CX - DIAL_CLEAR;
  int y0 = DIAL_CY - DIAL_CLEAR;
  int side = DIAL_CLEAR * 2 + 1;
  if (clearBg) drawPortraitRegion(x0, y0, side, side);
  const int cx = DIAL_CX;
  const int cy = DIAL_CY;
  const int r = DIAL_R;
  d->fillCircle(cx, cy, r, COL_FACE);
  d->drawCircle(cx, cy, r, COL_RIM);
  if (r >= 8) d->drawCircle(cx, cy, r - 1, 0x5A68);
  for (int i = 0; i < 12; i++) {
    float ang = (-90.0f + i * 30.0f) * (float)M_PI / 180.0f;
    int tx = cx + (int)((r - 2) * cosf(ang) + 0.5f);
    int ty = cy + (int)((r - 2) * sinf(ang) + 0.5f);
    d->drawPixel(tx, ty, COL_TICK);
  }
  int h = _dateTime->getHour();
  int m = _dateTime->getMinute();
  int sec = _dateTime->getSecond();
  float hourDeg = -90.0f + (h % 12) * 30.0f + m * 0.5f;
  float minDeg = -90.0f + m * 6.0f;
  drawHand(d, cx, cy, hourDeg, r - 5, COL_HAND_H);
  drawHand(d, cx, cy, minDeg, r - 3, COL_HAND_M);
  if (_showSeconds) {
    float secDeg = -90.0f + sec * 6.0f;
    drawHand(d, cx, cy, secDeg, r - 2, COL_HAND_S);
  }
  d->drawPixel(cx, cy, COL_RIM);
}

void Clockface::drawDialMorph() {
  if (!_dateTime) return;
  Adafruit_GFX* d = Locator::getDisplay();
  const int cx = DIAL_CX;
  const int cy = DIAL_CY;
  int r = _dialAnimR;
  if (r < 1) return;

  // soft trail ring as it spins away
  d->fillCircle(cx, cy, r + 1, COL_FACE);
  d->drawCircle(cx, cy, r, COL_RIM);
  if (r >= 3) d->drawCircle(cx, cy, r - 1, 0x5A68);

  float spin = (float)_dialAnimRot;
  for (int i = 0; i < 12; i++) {
    float ang = (-90.0f + i * 30.0f + spin) * (float)M_PI / 180.0f;
    int tx = cx + (int)((r - 1) * cosf(ang) + 0.5f);
    int ty = cy + (int)((r - 1) * sinf(ang) + 0.5f);
    if (tx >= 0 && tx < 64 && ty >= 0 && ty < 64) d->drawPixel(tx, ty, COL_TICK);
  }

  int h = _dateTime->getHour();
  int m = _dateTime->getMinute();
  float hourDeg = -90.0f + (h % 12) * 30.0f + m * 0.5f + spin * 1.4f;
  float minDeg = -90.0f + m * 6.0f + spin * 2.2f;
  int hLen = r > 4 ? r - 3 : r;
  int mLen = r > 2 ? r - 1 : r;
  drawHand(d, cx, cy, hourDeg, hLen, COL_HAND_H);
  drawHand(d, cx, cy, minDeg, mLen, COL_HAND_M);
  d->drawPixel(cx, cy, COL_RIM);
}

void Clockface::drawDigitScaled(int x, int y, uint8_t digit, uint8_t scale, uint16_t color) {
  if (digit > 10) return;
  Adafruit_GFX* d = Locator::getDisplay();
  for (int row = 0; row < 7; row++) {
    uint8_t bits = pgm_read_byte(&DIGIT_5X7[digit][row]);
    for (int col = 0; col < 5; col++) {
      if (!(bits & (1 << col))) continue;
      for (uint8_t sy = 0; sy < scale; sy++) {
        for (uint8_t sx = 0; sx < scale; sx++) {
          int xx = x + col * scale + sx;
          int yy = y + row * scale + sy;
          if (xx >= 0 && xx < 64 && yy >= 0 && yy < 64) d->drawPixel(xx, yy, color);
        }
      }
    }
  }
}

void Clockface::drawHourglass(int x, int y, int sec) {
  Adafruit_GFX* d = Locator::getDisplay();
  const uint16_t glass = 0xC616;
  const uint16_t sand = 0xFE60;
  const uint16_t sandDim = 0xD4A0;

  for (int i = 0; i < 11; i++) {
    d->drawPixel(x + i, y, glass);
    d->drawPixel(x + i, y + 17, glass);
  }
  for (int row = 1; row <= 7; row++) {
    int inset = row / 2;
    d->drawPixel(x + inset, y + row, glass);
    d->drawPixel(x + 10 - inset, y + row, glass);
    int brow = 17 - row;
    int binset = row / 2;
    d->drawPixel(x + binset, y + brow, glass);
    d->drawPixel(x + 10 - binset, y + brow, glass);
  }
  d->drawPixel(x + 4, y + 8, glass);
  d->drawPixel(x + 6, y + 8, glass);
  d->drawPixel(x + 4, y + 9, glass);
  d->drawPixel(x + 6, y + 9, glass);

  int topFill = (59 - sec) * 6 / 59;
  int botFill = sec * 6 / 59;
  if (sec == 0) { topFill = 6; botFill = 0; }
  if (sec >= 59) { topFill = 0; botFill = 6; }

  // top sand: fill upward from neck
  for (int n = 1; n <= topFill; n++) {
    int row = 8 - n;
    int inset = row / 2;
    for (int xx = x + inset + 1; xx <= x + 9 - inset; xx++) {
      d->drawPixel(xx, y + row, (n == topFill) ? sandDim : sand);
    }
  }
  // bottom sand: fill upward from base
  for (int n = 1; n <= botFill; n++) {
    int row = 16 - (n - 1);
    int fromBottom = 17 - row;
    int inset = fromBottom / 2;
    for (int xx = x + inset + 1; xx <= x + 9 - inset; xx++) {
      d->drawPixel(xx, y + row, sand);
    }
  }
  // trickle through neck
  if (sec > 0 && sec < 59) {
    d->drawPixel(x + 5, y + 8, sand);
    d->drawPixel(x + 5, y + 9, sandDim);
  }
}

void Clockface::drawDigitalClock() {
  if (!_dateTime) return;
  Adafruit_GFX* d = Locator::getDisplay();

  int8_t savedOx = _charOx;
  _charOx = 80;
  drawPortraitRegion(0, 0, 64, 64, false);
  _charOx = savedOx;

  int h = _dateTime->getHour();
  int m = _dateTime->getMinute();
  int s = _dateTime->getSecond();
  const uint16_t COL_DIG = 0xFFFF;
  const uint8_t sc = 2;
  int x = 7;
  int y = 18;

  drawDigitScaled(x, y, (h / 10) % 10, sc, COL_DIG);
  drawDigitScaled(x + 12, y, h % 10, sc, COL_DIG);

  if (_digitalColon) {
    d->fillRect(30, y + 3, 2, 2, COL_DIG);
    d->fillRect(30, y + 9, 2, 2, COL_DIG);
  }

  drawDigitScaled(x + 28, y, (m / 10) % 10, sc, COL_DIG);
  drawDigitScaled(x + 40, y, m % 10, sc, COL_DIG);

  // hourglass under the time — sand follows seconds
  drawHourglass(26, 40, s);
}

void Clockface::drawBlush(uint8_t level) {
  // Fixed cheek coords — never follow slide offset
  if (_slideMode != 0) return;
  Adafruit_GFX* d = Locator::getDisplay();
  uint16_t c = (level >= 2) ? BLUSH_COLOR : 0xF5D6;
  for (int i = 0; i < 2; i++) {
    int cx = (i == 0) ? BLUSH_LX : BLUSH_RX;
    int cy = BLUSH_Y;
    d->drawPixel(cx, cy, c);
    d->drawPixel(cx + 1, cy, c);
    if (level >= 2) {
      d->drawPixel(cx, cy + 1, c);
      d->drawPixel(cx + 1, cy + 1, 0xFAB2);
    }
  }
}

void Clockface::drawChar5x7(int x, int y, char ch, uint16_t color) {
  Adafruit_GFX* d = Locator::getDisplay();
  int idx = fontIndex(ch);
  for (int row = 0; row < 7; row++) {
    uint8_t bits = pgm_read_byte(&FONT5X7[idx][row]);
    for (int col = 0; col < 5; col++) {
      if (bits & (1 << (4 - col))) {
        d->drawPixel(x + col, y + row, color);
      }
    }
  }
}

void Clockface::drawText5x7(int x, int y, const char* s, uint16_t color) {
  int cx = x;
  for (const char* p = s; *p; p++) {
    if (*p == ' ') {
      cx += 4;
      continue;
    }
    drawChar5x7(cx, y, *p, color);
    cx += 6;
    if (cx > 58) break;
  }
}

void Clockface::drawBubble() {
  Adafruit_GFX* d = Locator::getDisplay();
  for (int y = 53; y <= 62; y++) {
    for (int x = 1; x <= 62; x++) {
      d->drawPixel(x, y, COL_BUBBLE_BG);
    }
  }
  char shown[12];
  strncpy(shown, _bubble, 10);
  shown[10] = 0;
  int len = strlen(shown);
  int w = len * 6;
  int x = (64 - w) / 2;
  if (x < 2) x = 2;
  drawText5x7(x, 55, shown, COL_BUBBLE);
}

void Clockface::drawHeart(int x, int y, uint16_t color) {
  // Compact 4x4 heart (no x-1 overhang — avoids leftover trails)
  Adafruit_GFX* d = Locator::getDisplay();
  d->drawPixel(x, y, color);
  d->drawPixel(x + 2, y, color);
  d->drawPixel(x, y + 1, color);
  d->drawPixel(x + 1, y + 1, color);
  d->drawPixel(x + 2, y + 1, color);
  d->drawPixel(x + 3, y + 1, color);
  d->drawPixel(x + 1, y + 2, color);
  d->drawPixel(x + 2, y + 2, color);
  d->drawPixel(x + 1, y + 3, color);
}

void Clockface::drawZ(int x, int y, uint16_t color) {
  Adafruit_GFX* d = Locator::getDisplay();
  d->drawPixel(x, y, color);
  d->drawPixel(x + 1, y, color);
  d->drawPixel(x + 2, y, color);
  d->drawPixel(x + 2, y + 1, color);
  d->drawPixel(x + 1, y + 2, color);
  d->drawPixel(x, y + 3, color);
  d->drawPixel(x, y + 4, color);
  d->drawPixel(x + 1, y + 4, color);
  d->drawPixel(x + 2, y + 4, color);
}

void Clockface::spawnSurpriseFx() {
  for (uint8_t i = 0; i < FX_N; i++) {
    _fx[i] = 10 + (i * 6) % 44;
    _fy[i] = 20 + (i * 5) % 28;
  }
  tickFxParticles();
}

void Clockface::spawnSleepFx() {
  for (uint8_t i = 0; i < FX_N; i++) {
    _fx[i] = 38 + (i % 3) * 5;
    _fy[i] = 18 + (i % 4) * 4;
  }
  tickFxParticles();
}

void Clockface::spawnHeartFx() {
  for (uint8_t i = 0; i < FX_N; i++) {
    _fx[i] = 14 + (i * 5) % 36;
    _fy[i] = 28 + (i % 3) * 6;
  }
  tickFxParticles();
}

void Clockface::tickFxParticles() {
  Adafruit_GFX* d = Locator::getDisplay();

  // Clear previous center heart pulse area (was leaving vertical trails)
  if (_effect == POKE_HEART) {
    drawPortraitRegion(28, 22, 10, 12);
  }

  for (uint8_t i = 0; i < FX_N; i++) {
    if (_fx[i] < 0) continue;
    drawPortraitRegion(_fx[i] - 1, _fy[i] - 1, 7, 7);

    _fy[i] -= 1;
    if (_effect == POKE_SLEEP && (i % 2) == 0) {
      _fx[i] += 1;
    }

    if (_fy[i] < 10) {
      if (_effect == POKE_SLEEP) {
        _fx[i] = 36 + ((i * 7 + millis()) % 16);
        _fy[i] = 30 + (i % 6);
      } else if (_effect == POKE_HEART) {
        _fx[i] = 12 + ((i * 9 + millis()) % 40);
        _fy[i] = 36 + (i % 8);
      } else {
        _fx[i] = 8 + ((i * 11 + millis()) % 48);
        _fy[i] = 40 + (i % 10);
      }
    }

    if (_fx[i] < 2) _fx[i] = 2;
    if (_fx[i] > 58) _fx[i] = 58;
    if (_fy[i] > 48) _fy[i] = 48;

    if (_effect == POKE_SLEEP) {
      drawZ(_fx[i], _fy[i], COL_Z);
    } else if (_effect == POKE_HEART) {
      drawHeart(_fx[i], _fy[i], COL_HEART);
    } else {
      if ((i % 2) == 0) drawHeart(_fx[i], _fy[i], COL_HEART);
      else {
        d->drawPixel(_fx[i], _fy[i], COL_SPARK);
        d->drawPixel(_fx[i] + 1, _fy[i], COL_CYAN);
      }
    }
  }

  if (_effect == POKE_HEART) {
    drawHeart(30, 26, COL_HEART);
    drawHeart(34, 28, COL_HEART);
  }
  if (_effect == POKE_SLEEP) {
    applySleepClosed();
  }
  drawTimeAnalog();
  if (_bubbleOn) drawBubble();
}

static const uint16_t COL_GAME_BG = 0x0842;
static const uint16_t COL_PADDLE = 0xFD78;
static const uint16_t COL_HUD = 0xFFFF;
static const uint16_t COL_SNAKE = 0x07E0;
static const uint16_t COL_SNAKE_H = 0xBFE0;
static const uint16_t COL_FOOD = 0xF80F;
static const uint16_t COL_LANE[4] = {0xB01F, 0x07FF, 0x07E0, 0xF800};
static const int PADDLE_W = 12;
static const int PADDLE_Y = 58;
static const int SN_CELL = 4;
static const int SN_OX = 0;
static const int SN_OY = 8;
static const int RH_HIT_Y = 50;
static const unsigned RH_BEAT = 420;

// short looping chart: lane 0-3, 255 = rest
static const uint8_t RH_CHART[] PROGMEM = {
  0, 1, 2, 3, 0, 2, 1, 3,
  0, 0, 1, 1, 2, 2, 3, 3,
  0, 3, 1, 2, 0, 1, 2, 3,
  0, 255, 2, 255, 1, 255, 3, 255,
};

void Clockface::gameDrawHud(const char* tag) {
  char buf[14];
  snprintf(buf, sizeof(buf), "%s%u", tag, (unsigned)_score);
  drawText5x7(2, 1, buf, COL_HUD);
  if (_gameMode == GM_RHYTHM) {
    snprintf(buf, sizeof(buf), "C%u", (unsigned)_combo);
    drawText5x7(40, 1, buf, COL_PADDLE);
  } else {
    snprintf(buf, sizeof(buf), "X%u", (unsigned)(_miss > 3 ? 0 : 3 - _miss));
    drawText5x7(44, 1, buf, COL_HUD);
  }
}

void Clockface::gameDrawOver() {
  Adafruit_GFX* d = Locator::getDisplay();
  d->fillScreen(COL_GAME_BG);
  drawText5x7(10, 22, "GAME", COL_HUD);
  drawText5x7(16, 32, "OVER", COL_HEART);
  char buf[12];
  snprintf(buf, sizeof(buf), "S:%u", (unsigned)_score);
  drawText5x7(18, 44, buf, COL_PADDLE);
}

void Clockface::gameStart(uint8_t mode) {
  _gameMode = mode;
  _gameOver = false;
  _score = 0;
  _miss = 0;
  _combo = 0;
  _effect = 0;
  _bubbleOn = false;
  _blinkFrame = -1;
  _gameTickMs = millis();
  PokeQueue::get().takeStickX();
  PokeQueue::get().takeStickY();
  PokeQueue::get().takeLane();

  if (mode == GM_CATCH) {
    _paddleX = 26;
    for (uint8_t i = 0; i < GH_N; i++) catchSpawn(i);
    catchDraw();
  } else if (mode == GM_SNAKE) {
    snakeStart();
  } else if (mode == GM_RHYTHM) {
    rhythmStart();
  }
}

void Clockface::gameQuit() {
  if (_gameMode == GM_OFF) return;
  _gameMode = GM_OFF;
  _gameOver = false;
  unsigned long now = millis();
  _nextSceneMs = now + SCENE_MS;
  _nextBlinkMs = now + 2000;
  _lastHour = _dateTime ? _dateTime->getHour() : -1;
  _lastMinute = _dateTime ? _dateTime->getMinute() : -1;
  drawFullFrame();
}

void Clockface::gameTick(unsigned long now) {
  if (_gameOver) {
    if (now - _gameTickMs > 4000) gameQuit();
    return;
  }
  if (_gameMode == GM_CATCH) catchTick(now);
  else if (_gameMode == GM_SNAKE) snakeTick(now);
  else if (_gameMode == GM_RHYTHM) rhythmTick(now);
}

void Clockface::catchSpawn(uint8_t i) {
  _gx[i] = 4 + (int8_t)((millis() * (17 + i * 13)) % 52);
  _gy[i] = (int8_t)(-4 - (i * 10));
  _gs[i] = 1 + (int8_t)((millis() + i * 9) % 3);
}

void Clockface::catchDraw() {
  Adafruit_GFX* d = Locator::getDisplay();
  d->fillScreen(COL_GAME_BG);
  gameDrawHud("S");
  for (uint8_t i = 0; i < GH_N; i++) {
    if (_gy[i] >= 0 && _gy[i] < 56) drawHeart(_gx[i], _gy[i], COL_HEART);
  }
  for (int x = 0; x < PADDLE_W; x++) {
    d->drawPixel(_paddleX + x, PADDLE_Y, COL_PADDLE);
    d->drawPixel(_paddleX + x, PADDLE_Y + 1, COL_PADDLE);
    d->drawPixel(_paddleX + x, PADDLE_Y + 2, COL_HEART);
  }
}

void Clockface::catchTick(unsigned long now) {
  int8_t sx = PokeQueue::get().takeStickX();
  if (sx != 0) {
    _paddleX += sx * 4;
    if (_paddleX < 1) _paddleX = 1;
    if (_paddleX > 64 - PADDLE_W - 1) _paddleX = 64 - PADDLE_W - 1;
  }
  if (now - _gameTickMs < 70) {
    catchDraw();
    return;
  }
  _gameTickMs = now;
  for (uint8_t i = 0; i < GH_N; i++) {
    _gy[i] += _gs[i];
    if (_gy[i] >= PADDLE_Y - 4 && _gy[i] <= PADDLE_Y + 1) {
      int hx = _gx[i] + 1;
      if (hx >= _paddleX - 1 && hx <= _paddleX + PADDLE_W) {
        if (_score < 99) _score++;
        catchSpawn(i);
        continue;
      }
    }
    if (_gy[i] > 62) {
      _miss++;
      catchSpawn(i);
      if (_miss >= 3) {
        _gameOver = true;
        _gameTickMs = now;
        gameDrawOver();
        return;
      }
    }
  }
  catchDraw();
}

void Clockface::snakePlaceFood() {
  for (int tries = 0; tries < 40; tries++) {
    uint8_t fx = (millis() + tries * 17) % SN_COLS;
    uint8_t fy = (millis() / 3 + tries * 11) % SN_ROWS;
    bool ok = true;
    for (uint8_t i = 0; i < _snLen; i++) {
      if (_snX[i] == fx && _snY[i] == fy) { ok = false; break; }
    }
    if (ok) { _foodX = fx; _foodY = fy; return; }
  }
  _foodX = 8; _foodY = 6;
}

void Clockface::snakeStart() {
  _snLen = 3;
  _snX[0] = 5; _snY[0] = 7;
  _snX[1] = 4; _snY[1] = 7;
  _snX[2] = 3; _snY[2] = 7;
  _snDir = 1;
  _snPend = 1;
  snakePlaceFood();
  snakeDraw();
}

void Clockface::snakeDraw() {
  Adafruit_GFX* d = Locator::getDisplay();
  d->fillScreen(COL_GAME_BG);
  gameDrawHud("S");
  // border
  for (int x = 0; x < 64; x++) {
    d->drawPixel(x, SN_OY - 1, 0x4208);
    d->drawPixel(x, SN_OY + SN_ROWS * SN_CELL, 0x4208);
  }
  for (uint8_t i = 0; i < _snLen; i++) {
    int px = SN_OX + _snX[i] * SN_CELL;
    int py = SN_OY + _snY[i] * SN_CELL;
    uint16_t c = (i == 0) ? COL_SNAKE_H : COL_SNAKE;
    for (int yy = 1; yy < SN_CELL - 1; yy++)
      for (int xx = 1; xx < SN_CELL - 1; xx++)
        d->drawPixel(px + xx, py + yy, c);
  }
  int fx = SN_OX + _foodX * SN_CELL + 1;
  int fy = SN_OY + _foodY * SN_CELL + 1;
  drawHeart(fx, fy, COL_FOOD);
}

void Clockface::snakeTick(unsigned long now) {
  // Backup: stick may arrive without going through action
  int8_t sx = PokeQueue::get().takeStickX();
  int8_t sy = PokeQueue::get().takeStickY();
  if (sx < 0 && _snDir != 1) _snPend = 0;
  if (sx > 0 && _snDir != 0) _snPend = 1;
  if (sy < 0 && _snDir != 3) _snPend = 2;
  if (sy > 0 && _snDir != 2) _snPend = 3;

  unsigned step = (_score > 20) ? 110 : (_score > 10 ? 140 : 180);
  if (now - _gameTickMs < step) return;
  _gameTickMs = now;
  _snDir = _snPend;

  int8_t nx = (int8_t)_snX[0];
  int8_t ny = (int8_t)_snY[0];
  if (_snDir == 0) nx--;
  else if (_snDir == 1) nx++;
  else if (_snDir == 2) ny--;
  else ny++;

  if (nx < 0 || ny < 0 || nx >= SN_COLS || ny >= SN_ROWS) {
    _gameOver = true; _gameTickMs = now; gameDrawOver(); return;
  }
  for (uint8_t i = 0; i < _snLen; i++) {
    if (_snX[i] == (uint8_t)nx && _snY[i] == (uint8_t)ny) {
      _gameOver = true; _gameTickMs = now; gameDrawOver(); return;
    }
  }

  bool eat = ((uint8_t)nx == _foodX && (uint8_t)ny == _foodY);
  if (!eat) {
    for (int i = _snLen - 1; i > 0; i--) {
      _snX[i] = _snX[i - 1];
      _snY[i] = _snY[i - 1];
    }
  } else {
    if (_snLen < SN_MAX) {
      for (int i = _snLen; i > 0; i--) {
        _snX[i] = _snX[i - 1];
        _snY[i] = _snY[i - 1];
      }
      _snLen++;
    } else {
      for (int i = _snLen - 1; i > 0; i--) {
        _snX[i] = _snX[i - 1];
        _snY[i] = _snY[i - 1];
      }
    }
    if (_score < 99) _score++;
    snakePlaceFood();
  }
  _snX[0] = (uint8_t)nx;
  _snY[0] = (uint8_t)ny;
  snakeDraw();
}

void Clockface::rhythmStart() {
  for (uint8_t i = 0; i < RH_N; i++) { _rhLane[i] = -1; _rhY[i] = 0; }
  _rhChartI = 0;
  _rhNextBeat = millis() + 600;
  _combo = 0;
  rhythmDraw();
}

void Clockface::rhythmSpawn() {
  uint8_t lane = pgm_read_byte(&RH_CHART[_rhChartI % sizeof(RH_CHART)]);
  _rhChartI++;
  if (lane == 255) return;
  for (uint8_t i = 0; i < RH_N; i++) {
    if (_rhLane[i] < 0) {
      _rhLane[i] = (int8_t)lane;
      _rhY[i] = 8;
      return;
    }
  }
}

void Clockface::rhythmDraw() {
  Adafruit_GFX* d = Locator::getDisplay();
  d->fillScreen(COL_GAME_BG);
  gameDrawHud("R");
  // 4 lanes
  for (int L = 0; L < 4; L++) {
    int x0 = 4 + L * 15;
    for (int y = 8; y < 56; y++) d->drawPixel(x0 + 6, y, 0x2104);
    // receptor
    for (int x = 0; x < 12; x++) {
      d->drawPixel(x0 + x, RH_HIT_Y, COL_LANE[L]);
      d->drawPixel(x0 + x, RH_HIT_Y + 1, COL_LANE[L]);
    }
  }
  for (uint8_t i = 0; i < RH_N; i++) {
    if (_rhLane[i] < 0) continue;
    int L = _rhLane[i];
    int x0 = 4 + L * 15 + 2;
    int y = _rhY[i];
    for (int yy = 0; yy < 4; yy++)
      for (int xx = 0; xx < 8; xx++)
        d->drawPixel(x0 + xx, y + yy, COL_LANE[L]);
  }
}

void Clockface::rhythmTick(unsigned long now) {
  // tap lanes
  uint8_t tap = PokeQueue::get().takeLane();
  if (tap >= 1 && tap <= 4) {
    int want = (int)tap - 1;
    bool hit = false;
    for (uint8_t i = 0; i < RH_N; i++) {
      if (_rhLane[i] != want) continue;
      int dy = _rhY[i] - RH_HIT_Y;
      if (dy >= -5 && dy <= 4) {
        _rhLane[i] = -1;
        if (_score < 99) _score++;
        if (_combo < 99) _combo++;
        hit = true;
        break;
      }
    }
    if (!hit) {
      _combo = 0;
      _miss++;
      if (_miss >= 8) {
        _gameOver = true; _gameTickMs = now; gameDrawOver(); return;
      }
    }
  }

  if (now >= _rhNextBeat) {
    _rhNextBeat = now + RH_BEAT;
    rhythmSpawn();
  }

  if (now - _gameTickMs < 40) {
    rhythmDraw();
    return;
  }
  _gameTickMs = now;

  for (uint8_t i = 0; i < RH_N; i++) {
    if (_rhLane[i] < 0) continue;
    _rhY[i] += 2;
    if (_rhY[i] > RH_HIT_Y + 6) {
      _rhLane[i] = -1;
      _combo = 0;
      _miss++;
      if (_miss >= 8) {
        _gameOver = true; _gameTickMs = now; gameDrawOver(); return;
      }
    }
  }
  rhythmDraw();
}
