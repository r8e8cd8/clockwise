#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Locator.h>
#include <IClockface.h>
#include <CWDateTime.h>
#include "assets.h"

#define CLOCKFACE_NAME "cw-cf-godwise"

enum GameMode : uint8_t {
  GM_OFF = 0,
  GM_CATCH = 1,
  GM_SNAKE = 2,
  GM_RHYTHM = 3,
};

class Clockface : public IClockface {
 private:
  Adafruit_GFX* _display;
  CWDateTime* _dateTime;

  unsigned long _lastAnimMs;
  unsigned long _nextBlinkMs;
  unsigned long _blinkStepMs;
  unsigned long _nextSceneMs;
  unsigned long _effectUntilMs;
  unsigned long _bubbleUntilMs;
  unsigned long _fxTickMs;
  unsigned long _gameTickMs;

  int8_t _blinkFrame;
  int _lastHour;
  int _lastMinute;
  uint8_t _blushLevel;
  uint8_t _sceneIdx;
  uint8_t _poolIdx;
  bool _wasDay;

  uint8_t _effect;
  char _bubble[28];
  bool _bubbleOn;
  bool _mottoOn;
  unsigned long _mottoUntilMs;
  uint8_t _moodLock;  // 0=none, else which action to auto-fire
  unsigned long _moodLockUntilMs;   // end of large time window
  unsigned long _moodLockNextMs;    // next auto trigger
  unsigned long _moodLockIntervalMs; // interval between triggers
  uint8_t _dayMode;  // 0=auto, 1=day, 2=night
  bool _showSeconds;
  int _lastSecond;

  // character slide + clock morph
  // 0 home | 1 leaving | 2 dialVanish | 3 digital | 4 returning | 5 calling
  int8_t _charOx;
  int8_t _charOy;
  int8_t _slideMode;
  int8_t _slideDir;   // +1 leave right, -1 leave left
  unsigned long _slideTickMs;
  int8_t _dialAnimR;
  int16_t _dialAnimRot;
  bool _digitalColon;
  uint8_t _callFrame;
  unsigned long _callUntilMs;

  // favorite backgrounds (persisted)
  static const uint8_t FAV_BYTES = 8;  // up to 64 scenes
  uint8_t _favBits[FAV_BYTES];
  bool _favOnly;

  // games
  uint8_t _gameMode;
  bool _gameOver;
  uint8_t _score;
  uint8_t _miss;

  // catch
  int8_t _paddleX;
  static const uint8_t GH_N = 3;
  int8_t _gx[GH_N];
  int8_t _gy[GH_N];
  int8_t _gs[GH_N];

  // snake
  static const uint8_t SN_MAX = 48;
  static const uint8_t SN_COLS = 16;
  static const uint8_t SN_ROWS = 14;
  uint8_t _snX[SN_MAX];
  uint8_t _snY[SN_MAX];
  uint8_t _snLen;
  int8_t _snDir;   // 0L 1R 2U 3D
  int8_t _snPend;
  uint8_t _foodX, _foodY;

  // rhythm
  static const uint8_t RH_N = 6;
  int8_t _rhLane[RH_N];
  int8_t _rhY[RH_N];
  uint8_t _rhChartI;
  unsigned long _rhNextBeat;
  uint8_t _combo;

  static const uint8_t FX_N = 8;
  int8_t _fx[FX_N];
  int8_t _fy[FX_N];

  void drawPortraitRegion(int x, int y, int w, int h);
  void drawPortraitRegion(int x, int y, int w, int h, bool skipDial);
  void drawFullFrame();
  void drawTimeAnalog(bool clearBg = true);
  void drawDialMorph();
  void drawDigitalClock();
  void drawHourglass(int x, int y, int sec);
  void drawDigitScaled(int x, int y, uint8_t digit, uint8_t scale, uint16_t color);
  void drawPatch(int eyeCx, int eyeCy, const uint16_t* patch);
  void applyBlinkFrame(int8_t frame);
  void restoreEyes();
  void drawBlush(uint8_t level);
  void tickAnimation(unsigned long now);
  bool isDayHour(int hour) const;
  void syncSceneToHour(bool forceRedraw);
  void advanceScene();
  void handlePoke(unsigned long now);
  void clearEffectArt();
  void clearBubbleArea();
  void drawBubble();
  void drawMotto();
  void showMotto(unsigned long now);
  void drawMottoGlyph(int x, int y, uint8_t glyphIdx, uint16_t color);
  void drawChar5x7(int x, int y, char ch, uint16_t color);
  void drawText5x7(int x, int y, const char* s, uint16_t color);
  void spawnSurpriseFx();
  void spawnSleepFx();
  void spawnHeartFx();
  void tickFxParticles();
  void drawHeart(int x, int y, uint16_t color);
  void drawZ(int x, int y, uint16_t color);
  void applyWinkClosed();
  void applySleepClosed();
  void applyPeekEyes();
  void setMoodLock(uint8_t mode, unsigned long now, uint16_t minutes, uint16_t intervalSec);
  void triggerMoodBurst(unsigned long now);
  void clearMoodLock(unsigned long now);
  void setDayMode(uint8_t mode);
  void setShowSeconds(bool on);
  void startSlideLeave(int8_t dir);
  void startSlideBack();
  void startCall();
  void tickSlide(unsigned long now);
  void beginDialVanish();
  void enterDigitalMode();
  bool charVisible() const;
  bool inPortraitUi() const;

  void loadFavorites();
  void saveFavorites();
  bool isFavorite(uint8_t idx) const;
  void setFavorite(uint8_t idx, bool on);
  void toggleFavoriteCurrent();
  void setFavOnly(bool on);
  uint8_t countFavoritesInPool(uint8_t base, uint8_t count) const;
  bool nextFavoriteInPool(uint8_t base, uint8_t count, uint8_t& outPoolIdx) const;
  void drawCallOverlay();

  void gameStart(uint8_t mode);
  void gameQuit();
  void gameTick(unsigned long now);
  void gameDrawOver();
  void gameDrawHud(const char* tag);

  void catchSpawn(uint8_t i);
  void catchTick(unsigned long now);
  void catchDraw();

  void snakePlaceFood();
  void snakeStart();
  void snakeTick(unsigned long now);
  void snakeDraw();

  void rhythmStart();
  void rhythmSpawn();
  void rhythmTick(unsigned long now);
  void rhythmDraw();

 public:
  Clockface(Adafruit_GFX* display);
  void setup(CWDateTime* dateTime);
  void selectScene(uint8_t idx);
  uint8_t sceneIndex() const { return _sceneIdx; }
  void update();
  void externalEvent(int type);
};
