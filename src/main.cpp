// FUD-X3 — passive counter-surveillance firmware for the XTEINK X3.
// Step 3 (increment 1): the UX shell plus passive BLE scanning feeding live
// detection counts into the Scan Summaries screen. No transmit, ever.

#include <Arduino.h>
#include <SPI.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "ui/Display.h"
#include "ui/Gfx.h"
#include "art/ZuseFaces.h"
#include "ui/Input.h"
#include "ui/App.h"
#include "screens/Screens.h"
#include "scan/Detections.h"
#include "scan/ScanBus.h"
#include "scan/ScanCoordinator.h"
#include "storage/SdStore.h"
#include "storage/Logger.h"
#include "time/Clock.h"
#include "scan/IgnoreList.h"
#include "scan/WifiApList.h"
#include "power/Fuel.h"
#include "Config.h"
#include "Version.h"

static Display display;
static Input input;
static App app(display, input);

static Detections detections;
static ScanCoordinator scanner;
static SdStore sd;
static Logger logger;
static Clock clk;
static Fuel fuel;

static ZuseScreen zuse;
static SettingsScreen settings;
static ScanSummaryScreen scans;
static CategoryScreen category;
static WifiNetScreen wifiNet;
static DetailScreen detail;
static SetClockScreen setClock;
static SdCardScreen sdCard;
static DeleteLogsScreen deleteLogs;
static OptionScreen option;
static IgnoreListScreen ignoreList;
static InfoScreen info;
static BatteryTestScreen batteryTest;
static AboutScreen about;

// Draw a full-screen message with a Zuse face (e-ink keeps it shown with no
// power) and deep-sleep until the power button is pressed, which reboots.
static void drawSleepAndHalt(const uint8_t* face, const char* titleMsg,
                             const char* subMsg) {
  Adafruit_GFX& c = display.cv();
  display.clear();
  int y = 140;
  c.drawBitmap((FUD_W - ZUSE_W) / 2, y, face, ZUSE_W, ZUSE_H, FUD_BLACK);
  y += ZUSE_H + 24;
  int w = gfx::textW(c, titleMsg, 7);
  y = gfx::clippedLine(c, (FUD_W - w) / 2, y, FUD_W - 8, titleMsg, 7,
                       FUD_BLACK) + 18;
  w = gfx::textW(c, subMsg, 3);
  gfx::clippedLine(c, (FUD_W - w) / 2, y, FUD_W - 8, subMsg, 3, FUD_BLACK);
  display.pushFull();
  delay(80);

  gpio_pullup_en(GPIO_NUM_3);  // power button idles high, pressed = low
  esp_deep_sleep_enable_gpio_wakeup(1ULL << 3, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();  // halts here; a press reboots into setup()
}

// Power button: a normal sleep/lock screen.
static void sleepNow() {
  drawSleepAndHalt(zuse_sleep, "Sleeping", "Press power to wake");
}

// Critical battery: warn and sleep before an ungraceful brownout. The message
// stays on the panel (no power needed) so a dead device still says why.
static void batteryDeadSleep() {
  drawSleepAndHalt(zuse_low, "Battery dead", "Charge me, then press power");
}

void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println(F("[FUD-X3] boot " FUDX3_VERSION));

  display.begin();
  // The e-ink driver inits SPI write-only (MISO disabled) and a second
  // SPI.begin() is a no-op once the bus is up, so tear it down and re-init with
  // MISO (GPIO7) enabled — the SD card shares SCLK/MOSI/MISO and must read.
  SPI.end();
  SPI.begin(8 /*SCLK*/, 7 /*MISO*/, 10 /*MOSI*/, 21 /*SS*/);
  input.begin();

  zuse.link(&settings, &scans, &detections, &sd);
  scans.link(&detections, &category, &wifiNet);
  category.link(&detections, &detail);
  detail.link(&detections);
  config.load();
  ignores.begin();
  wifiaps::begin();
  setClock.link(&clk);
  sdCard.link(&sd, &clk);
  deleteLogs.link(&sd, &clk);
  batteryTest.link(&sd, &clk);
  settings.link(&setClock, &sdCard, &deleteLogs, &option, &ignoreList, &info,
                &detections, &batteryTest, &about);

  // Bring the RTC and SD up before the first paint so the home screen shows the
  // correct state (not a transient "INSERT SD CARD").
  clk.begin();
  if (!clk.available()) Serial.println(F("[RTC] not found on I2C"));
  if (fuel.begin())
    Serial.printf("[FUEL] BQ27220 ok: %u%% %umV %+dmA\n", gBatt.pct, gBatt.mv,
                  gBatt.ma);
  else
    Serial.println(F("[FUEL] no fuel gauge on I2C"));
  logger.begin(&sd, &clk);
  if (sd.mount()) {
    logger.onMount();
    Serial.println(F("[FUD-X3] SD mounted; logging to /.fudx3/"));
  } else {
    Serial.println(F("[FUD-X3] no SD card; logging disabled"));
  }

  app.setHome(&zuse);
  app.setPowerHandler(sleepNow);
  app.setClock(&clk);
  app.render();

  scanner.begin();  // passive BLE + WiFi rotation on its own task
  Serial.println(F("[FUD-X3] ready; scanning BLE + WiFi"));
}

void loop() {
  const uint32_t now = millis();

  // Dev: 's' over serial dumps the current framebuffer as base64.
  if (Serial.available() && Serial.read() == 's') display.screenshot();

  // Drain classified events: update the live store, and log every new one.
  ScanEvent e;
  while (scanbus::receive(e)) {
    if (detections.apply(e, now)) logger.log(e, now);
  }
  detections.tick(now);

  // Battery: refresh the gauge, and sleep gracefully before a brownout so a
  // dead device still shows why (the e-ink message persists with no power).
  fuel.tick(now);
  // ...unless a screen asks to stay up (the battery test must run to the end).
  if (battCritical() && !(app.current() && app.current()->inhibitSleep()))
    batteryDeadSleep();

  // New calendar day: clear the cumulative count tally (lists are untouched).
  // Throttled so we aren't hammering the RTC over I2C.
  static uint32_t lastDayCheck = 0;
  static char lastDay[12] = {0};
  if (now - lastDayCheck > 30000) {
    lastDayCheck = now;
    char day[12];
    if (clk.isSet() && clk.dateStamp(day, sizeof(day))) {
      if (lastDay[0] && strcmp(day, lastDay) != 0) detections.resetCounts();
      strncpy(lastDay, day, sizeof(lastDay) - 1);
    }
  }

  // Detect a card inserted after boot, without blocking.
  static uint32_t lastSdCheck = 0;
  if (!sd.present() && now - lastSdCheck > 2000) {
    lastSdCheck = now;
    if (sd.tryRemount()) logger.onMount();
  }

  app.tick();  // input + redraw if dirty

  // Refresh a live screen (Scan Summaries) when its data changed, throttled so
  // the e-ink isn't driven constantly.
  static uint32_t lastSeq = 0;
  static uint32_t lastLive = 0;
  if (app.currentWantsLive() && detections.seq() != lastSeq &&
      now - lastLive > 3000) {
    lastSeq = detections.seq();
    lastLive = now;
    app.redraw();
  }

  // Wall-clock redraw for screens that ask for it (e.g. the battery test's
  // minute stopwatch), independent of detection activity.
  static uint32_t lastTimed = 0;
  const uint32_t tms = app.currentTimedRefreshMs();
  if (tms && now - lastTimed >= tms) {
    lastTimed = now;
    app.redraw();
  }

  delay(15);
}
