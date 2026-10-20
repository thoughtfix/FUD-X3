#include "Screens.h"
#include "../ui/Gfx.h"
#include "../Config.h"
#include "../Version.h"
#include "../scan/IgnoreList.h"
#include "../art/ZuseFaces.h"
#include "../power/Battery.h"
#include "../time/Clock.h"
#include "../storage/SdStore.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>  // strcasecmp
#include <SDCardManager.h>

// ---------- ListScreen ----------

static constexpr int LIST_SIZE = 4;     // text size for rows
static constexpr int ROW_PAD = 8;

void ListScreen::body(App& app, int topY, int bottomY) {
  Adafruit_GFX& c = app.display().cv();
  const int rowH = gfx::lineH(LIST_SIZE) + 2 * ROW_PAD;
  const int visible = (bottomY - topY) / rowH;
  const int n = count();
  if (visible <= 0 || n == 0) return;

  // Keep the highlighted row on screen.
  if (index_ < top_) top_ = index_;
  if (index_ >= top_ + visible) top_ = index_ - visible + 1;

  char buf[48];
  for (int row = 0; row < visible && (top_ + row) < n; ++row) {
    const int i = top_ + row;
    const int y = topY + row * rowH;
    const bool sel = (i == index_);
    if (sel) c.fillRect(0, y, FUD_W, rowH, FUD_BLACK);
    label(i, buf, sizeof(buf));
    gfx::clippedLine(c, ROW_PAD + 6, y + ROW_PAD, FUD_W - ROW_PAD, buf,
                     LIST_SIZE, sel ? FUD_WHITE : FUD_BLACK);
  }
}

// Controller-style layout: the left rocker (A/B) navigates, the right rocker
// (C/D) selects. Left thumb moves, right thumb acts.
void ListScreen::onButton(Btn b, App& app) {
  switch (b) {
    case Btn::A:  // up, wrapping from the first item to the last
      index_ = (index_ + count() - 1) % count();
      app.redraw();
      break;
    case Btn::B:  // down, wrapping from the last item to the first
      index_ = (index_ + 1) % count();
      app.redraw();
      break;
    case Btn::D:
      onSelect(index_, app);
      break;
    case Btn::C:
      app.back();
      break;
    default:
      break;  // sides inactive on lists (no next/previous screen here)
  }
}

void ListScreen::hints(const char* out[6]) {
  out[0] = "Up";    // A
  out[1] = "Down";  // B
  out[2] = "Back";  // C
  out[3] = "Open";  // D
  out[4] = "";      // Prev
  out[5] = "";      // Next
}

// ---------- ZuseScreen ----------

void ZuseScreen::body(App& app, int topY, int bottomY) {
  Adafruit_GFX& c = app.display().cv();
  int y = topY + 8;
  const bool noSd = sd_ && !sd_->present();

  // Zuse's reaction + (if the threat has an icon) the icon beside him. Driven
  // by hasAlert() so an acknowledged/baselined threat leaves Zuse calm.
  int face = ZFACE_HAPPY;
  int icon = -1;
  const uint8_t* faceBmp = nullptr;  // override (e.g. low battery, not in ZUSE_FACES)
  const char* mood = "All clear.";
  if (det_ && det_->hasAlert(Cat::Attack)) {
    face = ZFACE_ANGRY; icon = ZICON_ATTACK; mood = "Zuse sees an attack!";
  } else if (det_ && det_->hasAlert(Cat::HackDev)) {
    face = ZFACE_ANGRY; icon = ZICON_HACKER; mood = "A hacker tool is near.";
  } else if (det_ && det_->hasAlert(Cat::Camera)) {
    face = ZFACE_WARY; icon = ZICON_CAMERA; mood = "A camera is watching.";
  } else if (det_ && det_->hasAlert(Cat::Drone)) {
    face = ZFACE_WARY; mood = "A drone is up there.";
  } else if (det_ && det_->hasAlert(Cat::Tracker)) {
    face = ZFACE_WARY; mood = "A tracker is nearby.";
  } else if (battLow()) {
    // No threat to show, but the battery is getting low — let Zuse ask for a
    // charge (threats still take priority over this when present).
    faceBmp = zuse_low; mood = "Battery's low. Charge me soon.";
  }
  if (!faceBmp) faceBmp = ZUSE_FACES[face];

  if (noSd) {
    // No card: the face area becomes the prompt (detection still runs).
    y = gfx::wrapText(c, 16, y, FUD_W - 16, bottomY, "INSERT SD CARD", 5,
                      FUD_BLACK) + 8;
    y = gfx::wrapText(c, 16, y, FUD_W - 16, bottomY,
                      "Zuse keeps watch, but isn't logging.", 3, FUD_BLACK) + 14;
  } else {
    if (icon >= 0) {
      // Zuse on the left, the threat icon beside him on the right.
      const int gap = 20;
      const int totalW = ZUSE_W + gap + ICON_W;
      const int zx = (FUD_W - totalW) / 2;
      c.drawBitmap(zx, y, faceBmp, ZUSE_W, ZUSE_H, FUD_BLACK);
      c.drawBitmap(zx + ZUSE_W + gap, y + (ZUSE_H - ICON_H) / 2,
                   ZUSE_ICONS[icon], ICON_W, ICON_H, FUD_BLACK);
    } else {
      c.drawBitmap((FUD_W - ZUSE_W) / 2, y, faceBmp, ZUSE_W, ZUSE_H,
                   FUD_BLACK);
    }
    y += ZUSE_H + 6;
    const int mw = gfx::textW(c, mood, 3);
    y = gfx::clippedLine(c, (FUD_W - mw) / 2, y, FUD_W - 8, mood, 3, FUD_BLACK) + 12;
  }

  // Plain detection counts (always shown).
  if (det_) {
    const Cat cats[] = {Cat::Tracker, Cat::Camera,  Cat::Drone,
                        Cat::HackDev, Cat::Attack,   Cat::OpenWifi};
    char line[40];
    for (Cat cat : cats) {
      const int tot = det_->seenCount(cat);
      // Trackers are dominated by AirTags in public; show their share.
      if (cat == Cat::Tracker && tot > 0 && det_->airtagSeen() > 0) {
        snprintf(line, sizeof(line), "%s: %d (%d%% AirTag)", catName(cat), tot,
                 det_->airtagSeen() * 100 / tot);
      } else {
        snprintf(line, sizeof(line), "%s: %d", catName(cat), tot);
      }
      y = gfx::clippedLine(c, 40, y, FUD_W - 16, line, 3, FUD_BLACK) + 2;
    }
  }
}

bool ZuseScreen::alertActive() const {
  return det_ && (det_->hasAlert(Cat::Attack) || det_->hasAlert(Cat::HackDev) ||
                  det_->hasAlert(Cat::Camera) || det_->hasAlert(Cat::Drone) ||
                  det_->hasAlert(Cat::Tracker));
}

void ZuseScreen::onButton(Btn b, App& app) {
  if (b == Btn::B && alertActive()) {
    det_->baseline(millis());  // acknowledge: calm Zuse until something new
    app.redraw();
  } else if ((b == Btn::A || b == Btn::Prev) && settings_) {
    app.go(settings_);
  } else if ((b == Btn::D || b == Btn::Next) && scans_) {
    app.go(scans_);
  }
}

void ZuseScreen::hints(const char* out[6]) {
  out[0] = "Settings";                  // A
  out[1] = alertActive() ? "Ack" : "";  // B
  out[2] = "";                          // C
  out[3] = "Scans";                     // D
  out[4] = "Settings";  // Prev (left side) — explicit per UI spec
  out[5] = "Scans";     // Next (right side) — explicit per UI spec
}

// ---------- SettingsScreen ----------

static const char* kSettings[] = {
    "Set Clock",   "Logging",     "Auto Sleep",   "Baseline",     "Reset Counts",
    "Ignore List", "SD Card",     "Delete Logs",  "Battery Test", "About"};

int SettingsScreen::count() {
  return (int)(sizeof(kSettings) / sizeof(kSettings[0]));
}

void SettingsScreen::label(int i, char* buf, size_t n) {
  snprintf(buf, n, "%s", kSettings[i]);
}

static const char* const kLogOpts[] = {"On", "Off"};
static void pickLogging(int i) {
  config.logging = (i == 0);
  config.save();
}

static const char* const kSleepOpts[] = {"Never", "5 min", "15 min", "30 min"};
static const uint8_t kSleepMin[] = {0, 5, 15, 30};
static void pickSleep(int i) {
  config.sleepTimeoutMin = kSleepMin[i];
  config.save();
}

void SettingsScreen::onSelect(int i, App& app) {
  const char* item = kSettings[i];
  if (!strcmp(item, "Set Clock") && setClock_) {
    setClock_->open();
    app.go(setClock_);
  } else if (!strcmp(item, "SD Card") && sdCard_) {
    app.go(sdCard_);
  } else if (!strcmp(item, "Delete Logs") && deleteLogs_) {
    deleteLogs_->open();
    app.go(deleteLogs_);
  } else if (!strcmp(item, "Baseline") && det_ && info_) {
    det_->baseline(millis());
    info_->open("Baseline", "Set. Everything here is now known; only new "
                            "devices will stir Zuse.");
    app.go(info_);
  } else if (!strcmp(item, "Reset Counts") && det_ && info_) {
    det_->resetCounts();
    info_->open("Reset Counts", "Tally cleared. Counts start over from zero; "
                                "the lists of nearby devices are untouched.");
    app.go(info_);
  } else if (!strcmp(item, "Logging") && option_) {
    option_->open("Logging", kLogOpts, 2, config.logging ? 0 : 1, pickLogging);
    app.go(option_);
  } else if (!strcmp(item, "Auto Sleep") && option_) {
    int cur = 0;
    for (int k = 0; k < 4; ++k)
      if (kSleepMin[k] == config.sleepTimeoutMin) cur = k;
    option_->open("Auto Sleep", kSleepOpts, 4, cur, pickSleep);
    app.go(option_);
  } else if (!strcmp(item, "Battery Test") && batteryTest_) {
    batteryTest_->open();
    app.go(batteryTest_);
  } else if (!strcmp(item, "About") && info_) {
    info_->open("About", "FUD-X3 " FUDX3_VERSION
                         ". Passive, receive-only counter-surveillance "
                         "firmware. github.com/thoughtfix/FUD-X3");
    app.go(info_);
  } else if (!strcmp(item, "Ignore List") && ignoreList_) {
    ignoreList_->open();
    app.go(ignoreList_);
  } else if (info_) {
    info_->open(item, "Not built yet.");  // Ignore List / Zuse Theme: later
    app.go(info_);
  }
}

// ---------- OptionScreen ----------

void OptionScreen::body(App& app, int topY, int bottomY) {
  Adafruit_GFX& c = app.display().cv();
  const int rowH = gfx::lineH(4) + 16;
  for (int i = 0; i < n_; ++i) {
    const int y = topY + 8 + i * rowH;
    const bool s = (i == sel_);
    if (s) c.fillRect(0, y, FUD_W, rowH, FUD_BLACK);
    gfx::clippedLine(c, 14, y + 8, FUD_W - 14, opts_[i], 4,
                     s ? FUD_WHITE : FUD_BLACK);
  }
}

void OptionScreen::onButton(Btn b, App& app) {
  switch (b) {
    case Btn::A: sel_ = (sel_ + n_ - 1) % n_; app.redraw(); break;
    case Btn::B: sel_ = (sel_ + 1) % n_; app.redraw(); break;
    case Btn::D:
      if (onPick_) onPick_(sel_);
      app.back();
      break;
    case Btn::C: app.back(); break;
    default: break;
  }
}

void OptionScreen::hints(const char* out[6]) {
  out[0] = "Up";
  out[1] = "Down";
  out[2] = "Back";
  out[3] = "Select";
  out[4] = "";
  out[5] = "";
}

// ---------- IgnoreListScreen ----------

void IgnoreListScreen::body(App& app, int topY, int bottomY) {
  Adafruit_GFX& c = app.display().cv();
  const int n = ignores.count();
  if (n == 0) {
    gfx::wrapText(c, 16, topY + 8, FUD_W - 16, bottomY,
                  "No ignored devices. Open a device and press Ignore to add "
                  "your own gear.",
                  3, FUD_BLACK);
    return;
  }
  if (sel_ >= n) sel_ = n - 1;
  const int rowH = gfx::lineH(4) + 12;
  const int visible = (bottomY - topY) / rowH;
  int top = 0;
  if (sel_ >= visible) top = sel_ - visible + 1;
  char name[24];
  for (int row = 0; row < visible && (top + row) < n; ++row) {
    const int i = top + row;
    const int y = topY + 6 + row * rowH;
    const bool s = (i == sel_);
    if (s) c.fillRect(0, y, FUD_W, rowH, FUD_BLACK);
    if (ignores.nameAt(i, name, sizeof(name)))
      gfx::clippedLine(c, 14, y + 6, FUD_W - 14, name, 4,
                       s ? FUD_WHITE : FUD_BLACK);
  }
}

void IgnoreListScreen::onButton(Btn b, App& app) {
  const int n = ignores.count();
  switch (b) {
    case Btn::A: if (n) { sel_ = (sel_ + n - 1) % n; app.redraw(); } break;
    case Btn::B: if (n) { sel_ = (sel_ + 1) % n; app.redraw(); } break;
    case Btn::D: if (n) { ignores.removeAt(sel_); app.redraw(); } break;
    case Btn::C: app.back(); break;
    default: break;
  }
}

void IgnoreListScreen::hints(const char* out[6]) {
  const int n = ignores.count();
  out[0] = n ? "Up" : "";
  out[1] = n ? "Down" : "";
  out[2] = "Back";
  out[3] = n ? "Remove" : "";
  out[4] = "";
  out[5] = "";
}

// ---------- InfoScreen ----------

void InfoScreen::body(App& app, int topY, int bottomY) {
  gfx::wrapText(app.display().cv(), 16, topY + 8, FUD_W - 16, bottomY, msg_, 3,
                FUD_BLACK);
}

void InfoScreen::onButton(Btn b, App& app) {
  if (b == Btn::C || b == Btn::A) app.back();
}

void InfoScreen::hints(const char* out[6]) {
  out[0] = "";
  out[1] = "";
  out[2] = "Back";
  out[3] = "";
  out[4] = "";
  out[5] = "";
}

// ---------- SetClockScreen ----------

static uint8_t daysInMonth(uint16_t y, uint8_t m) {
  static const uint8_t d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  return (m >= 1 && m <= 12) ? d[m - 1] : 31;
}

void SetClockScreen::open() {
  field_ = 0;
  // Only fall back to the 2026-01-01 floor when the clock is unset or already
  // reads earlier than that; a clock already set to a later date is kept as-is.
  if (!clk_ || !clk_->get(t_) || !clk_->isSet() || t_.year < 2026)
    t_ = {2026, 1, 1, 0, 0, 0};
}

void SetClockScreen::adjust(int delta) {
  switch (field_) {
    case 0:
      t_.year = (uint16_t)(t_.year + delta);
      if (t_.year < 2026) t_.year = 2026;  // never set lower than 2026
      if (t_.year > 2099) t_.year = 2099;
      break;
    case 1:
      t_.mon = (uint8_t)((t_.mon - 1 + 12 + delta) % 12 + 1);
      break;
    case 2:
      t_.day = (uint8_t)((t_.day - 1 + 31 + delta) % 31 + 1);
      break;
    case 3:
      t_.hour = (uint8_t)((t_.hour + 24 + delta) % 24);
      break;
    case 4:
      t_.min = (uint8_t)((t_.min + 60 + delta) % 60);
      break;
  }
  const uint8_t dim = daysInMonth(t_.year, t_.mon);  // clamp day to month length
  if (t_.day > dim) t_.day = dim;
}

void SetClockScreen::body(App& app, int topY, int bottomY) {
  Adafruit_GFX& c = app.display().cv();
  const int X = 16;
  char date[16], time[8];
  snprintf(date, sizeof(date), "%04u-%02u-%02u", t_.year, t_.mon, t_.day);
  snprintf(time, sizeof(time), "%02u:%02u", t_.hour, t_.min);

  const int dateY = topY + 20;
  const int timeY = dateY + gfx::lineH(4) + 24;
  gfx::clippedLine(c, X, dateY, FUD_W - X, date, 4, FUD_BLACK);
  gfx::clippedLine(c, X, timeY, FUD_W - X, time, 4, FUD_BLACK);

  // Underline the active field, measuring prefixes so it lands correctly under
  // the proportional font.
  char pre[16];
  int rowY, startX, endX;
  auto widthTo = [&](const char* s, int n) {
    strncpy(pre, s, n);
    pre[n] = 0;
    return gfx::textW(c, pre, 4);
  };
  if (field_ <= 2) {
    rowY = dateY;
    const int off[] = {0, 5, 8}, len[] = {4, 2, 2};
    startX = X + widthTo(date, off[field_]);
    endX = X + widthTo(date, off[field_] + len[field_]);
  } else {
    rowY = timeY;
    const int off = (field_ == 3) ? 0 : 3;
    startX = X + widthTo(time, off);
    endX = X + widthTo(time, off + 2);
  }
  const int uy = rowY + gfx::lineH(4) - 2;
  c.fillRect(startX, uy, endX - startX, 4, FUD_BLACK);

  // The hint bar only labels the front buttons, so spell out the side buttons.
  const int hy = timeY + gfx::lineH(4) + 28;
  gfx::clippedLine(c, X, hy, FUD_W - X, "Side buttons pick the field", 3,
                   FUD_BLACK);
}

void SetClockScreen::onButton(Btn b, App& app) {
  switch (b) {
    case Btn::A: adjust(+1); app.redraw(); break;
    case Btn::B: adjust(-1); app.redraw(); break;
    case Btn::Prev: field_ = (field_ + 5 - 1) % 5; app.redraw(); break;  // prev field
    case Btn::Next: field_ = (field_ + 1) % 5; app.redraw(); break;      // next field
    case Btn::C: app.back(); break;  // Cancel: leave without saving
    case Btn::D:                     // Save
      if (clk_) {
        t_.sec = 0;
        clk_->set(t_);
      }
      app.back();
      break;
    default: break;
  }
}

void SetClockScreen::hints(const char* out[6]) {
  out[0] = "Up";      // A: raise the selected field
  out[1] = "Down";    // B: lower it
  out[2] = "Cancel";  // C
  out[3] = "Save";    // D
  out[4] = "Field";   // Prev (left side): previous field
  out[5] = "Field";   // Next (right side): next field
}

// ---------- SdCardScreen ----------

static const char* kLogDir = "/.fudx3";

void SdCardScreen::body(App& app, int topY, int bottomY) {
  Adafruit_GFX& c = app.display().cv();
  int y = topY + 8;
  char line[48];

  if (!sd_ || !sd_->present()) {
    y = gfx::wrapText(c, 16, y, FUD_W - 16, bottomY, "Card: not present", 4,
                      FUD_BLACK) + 8;
    gfx::wrapText(c, 16, y, FUD_W - 16, bottomY,
                  "Insert a card to log detections.", 4, FUD_BLACK);
    return;
  }

  y = gfx::clippedLine(c, 16, y, FUD_W - 16, "Card: present", 4, FUD_BLACK) + 10;

  // Sum the log files.
  auto files = SdMan.listFiles(kLogDir);
  int n = 0;
  uint64_t bytes = 0;
  for (auto& f : files) {
    if (!f.endsWith(".csv")) continue;
    char path[64];
    snprintf(path, sizeof(path), "%s/%s", kLogDir, f.c_str());
    bytes += sd_->fileSize(path);
    ++n;
  }
  snprintf(line, sizeof(line), "Logs: %d file(s), %lu KB", n,
           (unsigned long)(bytes / 1024));
  y = gfx::clippedLine(c, 16, y, FUD_W - 16, line, 4, FUD_BLACK) + 6;

  char ds[12];
  if (clk_ && clk_->dateStamp(ds, sizeof(ds)))
    snprintf(line, sizeof(line), "Today: %s.csv", ds);
  else
    snprintf(line, sizeof(line), "Clock not set (uptime log)");
  gfx::clippedLine(c, 16, y, FUD_W - 16, line, 4, FUD_BLACK);
}

void SdCardScreen::onButton(Btn b, App& app) {
  if (b == Btn::C || b == Btn::A) app.back();
}

void SdCardScreen::hints(const char* out[6]) {
  out[0] = "";
  out[1] = "";
  out[2] = "Back";
  out[3] = "";
  out[4] = "";
  out[5] = "";
}

// ---------- BatteryTestScreen ----------

void BatteryTestScreen::logRow(uint32_t elapsedS) {
  if (!sd_ || !sd_->present()) return;
  sd_->ensureDir(kLogDir);
  char path[48];
  snprintf(path, sizeof(path), "%s/battest.csv", kLogDir);
  if (!sd_->exists(path))
    sd_->appendLine(path, "elapsed_s,pct,mv,ma,refreshes,time");
  char ts[24];
  if (!clk_ || !clk_->stamp(ts, sizeof(ts))) snprintf(ts, sizeof(ts), "-");
  char row[96];
  snprintf(row, sizeof(row), "%lu,%u,%u,%d,%lu,%s", (unsigned long)elapsedS,
           gBatt.pct, gBatt.mv, gBatt.ma, (unsigned long)gPanelRefreshes, ts);
  sd_->appendLine(path, row);
}

void BatteryTestScreen::body(App& app, int topY, int bottomY) {
  Adafruit_GFX& c = app.display().cv();
  int y = topY + 8;
  char line[56];

  if (!gBatt.present) {
    gfx::wrapText(c, 16, y, FUD_W - 16, bottomY, "No fuel gauge detected.", 4,
                  FUD_BLACK);
    return;
  }

  const bool discharging = gBatt.valid && gBatt.ma < 0;

  if (!started_) {
    if (discharging) {  // unplugged — start the clock
      started_ = true;
      startMs_ = millis();
      lastLogMs_ = millis();
      startPct_ = gBatt.pct;
      logRow(0);
    } else {
      y = gfx::wrapText(c, 16, y, FUD_W - 16, bottomY,
                        "Unplug USB to start the test.", 4, FUD_BLACK) + 12;
      snprintf(line, sizeof(line), "Now: %u%%  %umV  %+dmA", gBatt.pct, gBatt.mv,
               gBatt.ma);
      y = gfx::clippedLine(c, 16, y, FUD_W - 16, line, 3, FUD_BLACK) + 10;
      gfx::wrapText(c, 16, y, FUD_W - 16, bottomY,
                    "Then let it run until the screen stops updating. Scanning "
                    "stays on; logs go to SD every minute.",
                    3, FUD_BLACK);
      return;
    }
  }

  const uint32_t elapsedS = (millis() - startMs_) / 1000;
  if (millis() - lastLogMs_ >= 60000) {  // minute cadence, matches refresh
    lastLogMs_ = millis();
    logRow(elapsedS);
  }

  // Big elapsed time (minute resolution — the panel only refreshes each minute).
  snprintf(line, sizeof(line), "%luh %02lum", (unsigned long)(elapsedS / 3600),
           (unsigned long)((elapsedS % 3600) / 60));
  y = gfx::clippedLine(c, 16, y, FUD_W - 16, line, 6, FUD_BLACK) + 14;
  snprintf(line, sizeof(line), "%u%% -> %u%%", startPct_, gBatt.pct);
  y = gfx::clippedLine(c, 16, y, FUD_W - 16, line, 4, FUD_BLACK) + 8;
  snprintf(line, sizeof(line), "%umV   %+dmA", gBatt.mv, gBatt.ma);
  y = gfx::clippedLine(c, 16, y, FUD_W - 16, line, 4, FUD_BLACK) + 12;
  const char* note = (gBatt.ma >= 0)      ? "Back on USB (charging)"
                     : (sd_ && sd_->present()) ? "Logging to SD each minute"
                                               : "Running (no SD — not logged)";
  gfx::clippedLine(c, 16, y, FUD_W - 16, note, 3, FUD_BLACK);
}

void BatteryTestScreen::onButton(Btn b, App& app) {
  if (b == Btn::C || b == Btn::A) {
    app.back();
  } else if (b == Btn::B) {  // restart
    started_ = false;
    app.redraw();
  }
}

void BatteryTestScreen::hints(const char* out[6]) {
  out[0] = "";
  out[1] = started_ ? "Restart" : "";
  out[2] = "Back";
  out[3] = "";
  out[4] = "";
  out[5] = "";
}

// ---------- DeleteLogsScreen ----------

// Julian day number helpers for date arithmetic (This Week = last 7 days).
static long toJDN(int y, int m, int d) {
  long a = (14 - m) / 12, yy = y + 4800 - a, mm = m + 12 * a - 3;
  return d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 -
         32045;
}
static void fromJDN(long jdn, int& y, int& m, int& d) {
  long a = jdn + 32044, b = (4 * a + 3) / 146097, c = a - (146097 * b) / 4;
  long dd = (4 * c + 3) / 1461, e = c - (1461 * dd) / 4, mm = (5 * e + 2) / 153;
  d = (int)(e - (153 * mm + 2) / 5 + 1);
  m = (int)(mm + 3 - 12 * (mm / 10));
  y = (int)(100 * b + dd - 4800 + mm / 10);
}

void DeleteLogsScreen::open() {
  sel_ = 0;
  confirming_ = false;
  done_ = false;
  deleted_ = 0;
  scopeN_ = 0;
  // Today/This Week only mean something with the clock set.
  if (clk_ && clk_->isSet()) {
    scopeName_[scopeN_] = "Today";
    scopeKind_[scopeN_++] = 0;
    scopeName_[scopeN_] = "This Week";
    scopeKind_[scopeN_++] = 1;
  }
  scopeName_[scopeN_] = "All";
  scopeKind_[scopeN_++] = 2;
}

int DeleteLogsScreen::doDelete(int kind) {
  if (!sd_ || !sd_->present()) return 0;
  int removed = 0;
  char path[64];

  if (kind == 2) {  // All: every .csv in the log dir
    auto files = SdMan.listFiles(kLogDir);
    for (auto& f : files) {
      if (!f.endsWith(".csv")) continue;
      snprintf(path, sizeof(path), "%s/%s", kLogDir, f.c_str());
      if (sd_->remove(path)) ++removed;
    }
    return removed;
  }

  RtcTime t;
  if (!clk_ || !clk_->get(t)) return 0;
  const long today = toJDN(t.year, t.mon, t.day);
  const int days = (kind == 0) ? 1 : 7;  // Today vs This Week
  for (int n = 0; n < days; ++n) {
    int y, m, d;
    fromJDN(today - n, y, m, d);
    snprintf(path, sizeof(path), "%s/%04d%02d%02d.csv", kLogDir, y, m, d);
    if (sd_->exists(path) && sd_->remove(path)) ++removed;
  }
  return removed;
}

void DeleteLogsScreen::body(App& app, int topY, int bottomY) {
  Adafruit_GFX& c = app.display().cv();

  if (done_) {
    char line[40];
    snprintf(line, sizeof(line), "Deleted %d file(s).", deleted_);
    gfx::wrapText(c, 16, topY + 8, FUD_W - 16, bottomY, line, 3, FUD_BLACK);
    return;
  }
  if (confirming_) {
    char line[48];
    snprintf(line, sizeof(line), "Delete %s logs?", scopeName_[sel_]);
    int y = gfx::wrapText(c, 16, topY + 8, FUD_W - 16, bottomY, line, 3,
                          FUD_BLACK) + 10;
    gfx::wrapText(c, 16, y, FUD_W - 16, bottomY, "This cannot be undone.", 2,
                  FUD_BLACK);
    return;
  }
  // Scope list.
  const int rowH = gfx::lineH(4) + 16;
  for (int i = 0; i < scopeN_; ++i) {
    const int y = topY + 8 + i * rowH;
    const bool s = (i == sel_);
    if (s) c.fillRect(0, y, FUD_W, rowH, FUD_BLACK);
    gfx::clippedLine(c, 14, y + 8, FUD_W - 14, scopeName_[i], 4,
                     s ? FUD_WHITE : FUD_BLACK);
  }
}

void DeleteLogsScreen::onButton(Btn b, App& app) {
  if (done_) {
    if (b == Btn::C || b == Btn::A) app.back();
    return;
  }
  if (confirming_) {
    if (b == Btn::D) {
      deleted_ = doDelete(scopeKind_[sel_]);
      done_ = true;
      app.redraw();
    } else if (b == Btn::C) {
      confirming_ = false;
      app.redraw();
    }
    return;
  }
  switch (b) {
    case Btn::A: sel_ = (sel_ + scopeN_ - 1) % scopeN_; app.redraw(); break;
    case Btn::B: sel_ = (sel_ + 1) % scopeN_; app.redraw(); break;
    case Btn::D: confirming_ = true; app.redraw(); break;
    case Btn::C: app.back(); break;
    default: break;
  }
}

void DeleteLogsScreen::hints(const char* out[6]) {
  for (int i = 0; i < 6; ++i) out[i] = "";
  if (done_) {
    out[2] = "Back";
  } else if (confirming_) {
    out[2] = "No";   // C
    out[3] = "Yes";  // D
  } else {
    out[0] = "Up";      // A
    out[1] = "Down";    // B
    out[2] = "Back";    // C
    out[3] = "Delete";  // D
  }
}

// ---------- ScanSummaryScreen ----------

// Rows map to a detection category; the last row is a live Wi-Fi view (no count).
struct Row { const char* name; Cat cat; };
static const Row kRows[] = {
    {"Cameras", Cat::Camera},         {"Drones", Cat::Drone},
    {"Hacker Devices", Cat::HackDev}, {"Attacks", Cat::Attack},
    {"Trackers", Cat::Tracker},       {"Open Wi-Fi", Cat::OpenWifi},
    {"Wi-Fi Networks", Cat::None}};

int ScanSummaryScreen::count() {
  return (int)(sizeof(kRows) / sizeof(kRows[0]));
}

void ScanSummaryScreen::label(int i, char* buf, size_t n) {
  if (kRows[i].cat == Cat::None) {
    snprintf(buf, n, "%s  >", kRows[i].name);  // Wi-Fi: a view, not a count
    return;
  }
  const int c = det_ ? det_->seenCount(kRows[i].cat) : 0;
  snprintf(buf, n, "%s  (%d)", kRows[i].name, c);
}

void ScanSummaryScreen::onSelect(int i, App& app) {
  if (kRows[i].cat == Cat::None) {  // the live Wi-Fi list
    if (wifi_) app.go(wifi_);
    return;
  }
  if (!cat_) return;
  cat_->open(kRows[i].cat);
  app.go(cat_);
}

// ---------- WifiNetScreen ----------

void WifiNetScreen::body(App& app, int topY, int bottomY) {
  if (count() == 0) {
    gfx::wrapText(app.display().cv(), 16, topY + 8, FUD_W - 16, bottomY,
                  "No networks yet. Wi-Fi is scanned every few seconds - "
                  "hang on while Zuse looks around.",
                  3, FUD_BLACK);
    return;
  }
  ListScreen::body(app, topY, bottomY);
}

int WifiNetScreen::count() {
  n_ = wifiaps::snapshot(aps_, wifiaps::MAXN);  // refresh the cached snapshot
  return n_;
}

void WifiNetScreen::label(int i, char* buf, size_t n) {
  if (i < 0 || i >= n_) {
    snprintf(buf, n, "-");
    return;
  }
  snprintf(buf, n, "%s  %ddB%s", aps_[i].ssid, aps_[i].rssi,
           aps_[i].open ? "  open" : "");
}

// ---------- CategoryScreen ----------

int CategoryScreen::count() {
  const int c = det_ ? det_->count(cat_) : 0;
  n_ = 0;
  for (int k = 0; k < c && n_ < MAXROWS; ++k) {
    if (det_->viewAt(cat_, k, views_[n_])) {
      slot_[n_] = k;
      ++n_;
    }
  }
  // Open Wi-Fi is a reference list, so order it by network name, not by when
  // each was last seen. Other categories keep store order (newest-ish first).
  if (cat_ == Cat::OpenWifi) {
    for (int i = 1; i < n_; ++i) {
      DetView v = views_[i];
      int s = slot_[i];
      int j = i - 1;
      while (j >= 0 && strcasecmp(views_[j].name, v.name) > 0) {
        views_[j + 1] = views_[j];
        slot_[j + 1] = slot_[j];
        --j;
      }
      views_[j + 1] = v;
      slot_[j + 1] = s;
    }
  }
  return n_;
}

// Humanize a duration, and an emphasis marker that grows the longer a device
// has been hanging around (stuck-with-you).
static void humanDur(uint32_t ms, char* out, size_t n) {
  uint32_t s = ms / 1000;
  if (s < 60) snprintf(out, n, "%lus", (unsigned long)s);
  else if (s < 3600) snprintf(out, n, "%lum", (unsigned long)(s / 60));
  else snprintf(out, n, "%luh", (unsigned long)(s / 3600));
}
static const char* emphasis(uint32_t presentMs) {
  const uint32_t m = presentMs / 60000;
  if (m >= 30) return "  !!";
  if (m >= 10) return "  !";
  if (m >= 2) return "  *";
  return "";
}

void CategoryScreen::label(int i, char* buf, size_t n) {
  if (i < 0 || i >= n_) {
    snprintf(buf, n, "-");
    return;
  }
  const DetView& v = views_[i];
  const uint32_t present = millis() - v.firstSeen;
  char dur[12];
  humanDur(present, dur, sizeof(dur));
  // For open Wi-Fi the SSID (name) is the useful identifier, not "Open".
  const char* lbl = (cat_ == Cat::OpenWifi && v.name[0]) ? v.name : v.label;
  if (v.collapsed)
    snprintf(buf, n, "%s (%d)  %s%s", lbl, v.collapsedCount, dur,
             emphasis(present));
  else
    snprintf(buf, n, "%s  %s%s", lbl, dur, emphasis(present));
}

void CategoryScreen::onSelect(int i, App& app) {
  if (!detail_ || i < 0 || i >= n_) return;
  detail_->open(cat_, slot_[i]);  // map display row back to its store index
  app.go(detail_);
}

// ---------- DetailScreen ----------

void DetailScreen::body(App& app, int topY, int bottomY) {
  Adafruit_GFX& c = app.display().cv();
  DetView v;
  if (!det_ || !det_->viewAt(cat_, index_, v)) {
    gfx::wrapText(c, 16, topY + 8, FUD_W - 16, bottomY,
                  "This detection is no longer active.", 3, FUD_BLACK);
    return;
  }

  int y = topY + 8;
  // Device label, large.
  y = gfx::wrapText(c, 16, y, FUD_W - 16, bottomY, v.label, 4, FUD_BLACK) + 6;

  char line[64];
  // Advertised name / SSID, when present and not identical to the label.
  if (v.name[0] && strcmp(v.name, v.label) != 0) {
    snprintf(line, sizeof(line), "\"%s\"", v.name);
    y = gfx::wrapText(c, 16, y, FUD_W - 16, bottomY, line, 3, FUD_BLACK) + 8;
  }

  const uint32_t now = millis();
  const uint32_t ageS = (now - v.lastSeen) / 1000;

  if (v.collapsed) {
    snprintf(line, sizeof(line), "%d beacons detected", v.collapsedCount);
    y = gfx::wrapText(c, 16, y, FUD_W - 16, bottomY, line, 3, FUD_BLACK) + 4;
    y = gfx::wrapText(c, 16, y, FUD_W - 16, bottomY,
                      "AirTags rotate their address, so they are counted, not "
                      "tracked individually.",
                      2, FUD_BLACK) + 8;
  } else {
    snprintf(line, sizeof(line), "MAC %02X:%02X:%02X:%02X:%02X:%02X", v.key[0],
             v.key[1], v.key[2], v.key[3], v.key[4], v.key[5]);
    y = gfx::clippedLine(c, 16, y, FUD_W - 16, line, 3, FUD_BLACK) + 4;
    snprintf(line, sizeof(line), "Signal  %d dBm", v.rssi);
    y = gfx::clippedLine(c, 16, y, FUD_W - 16, line, 3, FUD_BLACK) + 4;
  }

  const uint32_t present = now - v.firstSeen;
  char dur[12];
  humanDur(present, dur, sizeof(dur));
  snprintf(line, sizeof(line), "First seen  %s ago%s", dur, emphasis(present));
  y = gfx::clippedLine(c, 16, y, FUD_W - 16, line, 3, FUD_BLACK) + 4;
  snprintf(line, sizeof(line), "Last seen  %lus ago", (unsigned long)ageS);
  y = gfx::clippedLine(c, 16, y, FUD_W - 16, line, 3, FUD_BLACK) + 4;
  snprintf(line, sizeof(line), "Seen  %lu times", (unsigned long)v.hits);
  y = gfx::clippedLine(c, 16, y, FUD_W - 16, line, 3, FUD_BLACK) + 4;
  if (!v.collapsed) {
    snprintf(line, sizeof(line), "Confidence  %c", v.conf);
    y = gfx::clippedLine(c, 16, y, FUD_W - 16, line, 3, FUD_BLACK) + 4;
  }
  if (present >= 10 * 60000)
    gfx::wrapText(c, 16, y + 4, FUD_W - 16, bottomY,
                  "This device has been near you a while.", 3, FUD_BLACK);
}

void DetailScreen::onButton(Btn b, App& app) {
  const int n = det_ ? det_->count(cat_) : 0;
  switch (b) {
    case Btn::A:      // Prev device
    case Btn::Prev:
      if (n > 0) { index_ = (index_ + n - 1) % n; app.redraw(); }
      break;
    case Btn::B:      // Next device
    case Btn::Next:
      if (n > 0) { index_ = (index_ + 1) % n; app.redraw(); }
      break;
    case Btn::C:
      app.back();
      break;
    case Btn::D: {  // Ignore this device by its advertised name (if any)
      DetView v;
      if (det_ && det_->viewAt(cat_, index_, v) && v.name[0]) {
        ignores.add(v.name);
        app.back();
      }
      break;
    }
    default:
      break;
  }
}

void DetailScreen::hints(const char* out[6]) {
  out[0] = "Prev";    // A
  out[1] = "Next";    // B
  out[2] = "Back";    // C
  out[3] = "Ignore";  // D (only acts when the device has a name)
  out[4] = "Prev";    // Prev (side) — also steps devices
  out[5] = "Next";    // Next (side)
}
