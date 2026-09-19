#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Clockface
#include <Clockface.h>
// Commons
#include <WiFiController.h>
#include <CWDateTime.h>
#include <CWPreferences.h>
#include <CWWebServer.h>
#include <StatusController.h>
#include <birthday_boot.h>

#define MIN_BRIGHT_DISPLAY_ON 4
#define MIN_BRIGHT_DISPLAY_OFF 0

#define ESP32_LED_BUILTIN 2

MatrixPanel_I2S_DMA *dma_display = nullptr;

Clockface *clockface;

WiFiController wifi;
CWDateTime cwDateTime;

long autoBrightMillis = 0;
uint8_t currentBrightSlot = -1;

static void drawBootFrame(Adafruit_GFX *d, uint8_t idx)
{
  if (!d || idx >= BIRTHDAY_BOOT_COUNT) return;
  for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 64; x++) {
      uint8_t c = pgm_read_byte(&BIRTHDAY_BOOT[idx][y * 64 + x]);
      d->drawPixel(x, y, birthdayRgb565(c));
    }
  }
}

// Cake + loading bar together
void playCakeWithLoadingBar(Adafruit_GFX *d, unsigned long ms)
{
  if (!d || BIRTHDAY_BOOT_COUNT < 2) return;
  // [0]=person blow, [1]=cake
  drawBootFrame(d, 1);

  const uint16_t barBg = 0x2104;
  const uint16_t barFg = 0xF81F;
  for (int x = 0; x < 64; x++) {
    d->drawPixel(x, 61, barBg);
    d->drawPixel(x, 62, barBg);
  }
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    int w = (int)((millis() - t0) * 64 / ms);
    if (w > 64) w = 64;
    for (int x = 0; x < w; x++) {
      d->drawPixel(x, 61, barFg);
      d->drawPixel(x, 62, barFg);
    }
    delay(16);
  }
}

// Birthday character (blow / person) — no Happy Birthday text
void playBirthdayPortrait(Adafruit_GFX *d)
{
  if (!d || BIRTHDAY_BOOT_COUNT == 0) return;
  d->fillScreen(0x0000);  // wipe cake text + loading bar
  drawBootFrame(d, 0);
  delay(BIRTHDAY_BOOT_MS);
}

// Order: cake+loading bar → birthday person → caller shows normal face
void playBirthdayBoot(Adafruit_GFX *d)
{
  playCakeWithLoadingBar(d, 2500);
  playBirthdayPortrait(d);
}

bool isValidI2SSpeed(uint32_t speed) {
  return speed == 8000000 || speed == 16000000 || speed == 20000000;
}

bool isValidDriver(uint32_t drv) {
  return drv >= 0 && drv <= 5;
}

// This board has no P18; E uses P2 (LED pin — StatusController LED blink disabled)
static const int8_t PANEL_E_PIN = 2;

void showScanBands(Adafruit_GFX *d)
{
  d->fillRect(0, 0, 64, 16, 0xF800);
  d->fillRect(0, 16, 64, 16, 0x07E0);
  d->fillRect(0, 32, 64, 16, 0x001F);
  d->fillRect(0, 48, 64, 16, 0xFFE0);
  delay(3000);
}

void displaySetup(bool swapBlueGreen, bool swapBlueRed, uint8_t displayBright, uint8_t displayRotation, uint8_t driver, uint32_t i2cSpeed, uint8_t E_pin)
{
  (void)swapBlueGreen;
  (void)swapBlueRed;
  (void)E_pin;

  HUB75_I2S_CFG mxconfig(64, 64, 1);
  mxconfig.gpio.r1 = 27;
  mxconfig.gpio.g1 = 25;
  mxconfig.gpio.b1 = 26;
  mxconfig.gpio.r2 = 13;
  mxconfig.gpio.g2 = 14;
  mxconfig.gpio.b2 = 12;
  mxconfig.gpio.a = 23;
  mxconfig.gpio.b = 19;
  mxconfig.gpio.c = 5;
  mxconfig.gpio.d = 17;
  mxconfig.gpio.e = PANEL_E_PIN;
  mxconfig.gpio.lat = 4;
  mxconfig.gpio.oe = 15;
  mxconfig.gpio.clk = 16;
  mxconfig.clkphase = false;
  mxconfig.driver = HUB75_I2S_CFG::SHIFTREG;
  mxconfig.latch_blanking = 1;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;  // stable refresh; avoid prefs garbage

  if (isValidDriver(driver) && driver == 0) {
    mxconfig.driver = HUB75_I2S_CFG::SHIFTREG;
  }
  (void)i2cSpeed;

  Serial.printf("[PANEL] 64x64 E=GPIO%d bright=%u\n", PANEL_E_PIN, displayBright);

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(displayBright);
  dma_display->clearScreen();
  dma_display->setRotation(displayRotation);
}

void automaticBrightControl()
{
  // autoBrightMax > 0 means calibrated / enabled (live, no reboot needed)
  auto *p = ClockwiseParams::getInstance();
  if (p->autoBrightMax == 0) return;

  if (millis() - autoBrightMillis > 3000)
  {
    int16_t currentValue = analogRead(p->ldrPin);

    uint16_t ldrMin = p->autoBrightMin;
    uint16_t ldrMax = p->autoBrightMax;
    if (ldrMax <= ldrMin) ldrMax = ldrMin + 1;

    const uint8_t minBright = (currentValue < ldrMin ? MIN_BRIGHT_DISPLAY_OFF : MIN_BRIGHT_DISPLAY_ON);
    uint8_t maxBright = p->displayBright;

    uint8_t slots = 10;
    uint8_t mapLDR = map(currentValue > ldrMax ? ldrMax : currentValue, ldrMin, ldrMax, 1, slots);
    uint8_t mapBright = map(mapLDR, 1, slots, minBright, maxBright);

    if (abs(currentBrightSlot - mapLDR) >= 2 || mapBright == 0) {
      dma_display->setBrightness8(mapBright);
      currentBrightSlot = mapLDR;
    }
    autoBrightMillis = millis();
  }
}

void setup()
{
  Serial.begin(115200);
  // Do not touch GPIO2 as LED — it is HUB75 E on this board

  ClockwiseParams::getInstance()->load();
  pinMode(ClockwiseParams::getInstance()->ldrPin, INPUT);

  auto *p = ClockwiseParams::getInstance();
  if (!p->preferences.isKey("marioPortraitDim")) {
    p->E_pin = PANEL_E_PIN;
    p->displayBright = 18;
    p->autoBrightMax = 0;
    p->timeZone = "Asia/Shanghai";
    p->use24hFormat = true;
    p->ntpServer = "ntp.aliyun.com";
    p->save();
    p->preferences.putBool("marioPortraitDim", true);
    Serial.println("[CONFIG] Portrait dim brightness 18");
  }
  p->E_pin = PANEL_E_PIN;
  p->timeZone = "Asia/Shanghai";
  p->displayBright = 14;
  // Keep user's LDR auto-bright prefs (do not force autoBrightMax=0 every boot)
  if (p->ldrPin == 0) p->ldrPin = 35;
  p->driver = 0;
  p->i2cSpeed = 10000000;
  p->save();

  displaySetup(false, false, p->displayBright, p->displayRotation, p->driver, p->i2cSpeed, PANEL_E_PIN);
  clockface = new Clockface(dma_display);

  Serial.printf("[LDR] pin=GPIO%u enabled=%d min=%u max=%u\n",
                p->ldrPin, (p->autoBrightMax > 0) ? 1 : 0, p->autoBrightMin, p->autoBrightMax);

  // 1) loading bar  2) birthday portrait  3) normal clockface
  // (skip WiFi icon screens so they don't interrupt the gift intro)
  playBirthdayBoot(dma_display);

  if (wifi.begin())
  {
    cwDateTime.begin(p->timeZone.c_str(),
                     p->use24hFormat,
                     p->ntpServer.c_str(),
                     p->manualPosix.c_str());
    Serial.printf("[POKE] Phone UI: http://%s/poke\n", WiFi.localIP().toString().c_str());
  }
  clockface->setup(&cwDateTime);
}

void loop()
{
  wifi.handleImprovWiFi();

  if (wifi.isConnected())
  {
    ClockwiseWebServer::getInstance()->handleHttpRequest();
    ezt::events();
  }

  if (wifi.connectionSucessfulOnce)
  {
    clockface->update();
  }

  automaticBrightControl();
}
