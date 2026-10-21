// The FUD-X3 shell screens: the Zuse home, Settings, and Scan Summaries.
// Step-1 scope: real navigation and layout, placeholder content (no radios,
// no SD, no Zuse art yet).
#pragma once
#include "../ui/App.h"
#include "../scan/Detections.h"
#include "../scan/WifiApList.h"
#include "../time/Clock.h"
#include "../storage/SdStore.h"

// A scrollable, single-select list. Subclasses supply the items.
class ListScreen : public Screen {
 public:
  void body(App& app, int topY, int bottomY) override;
  void onButton(Btn b, App& app) override;
  void hints(const char* out[6]) override;

 protected:
  virtual int count() = 0;
  virtual void label(int i, char* buf, size_t n) = 0;
  virtual void onSelect(int /*i*/, App& /*app*/) {}  // B on an item

  int index_ = 0;
  int top_ = 0;  // first visible row (scroll offset)
};

class ZuseScreen : public Screen {
 public:
  void link(Screen* settings, Screen* scans, Detections* det, SdStore* sd) {
    settings_ = settings;
    scans_ = scans;
    det_ = det;
    sd_ = sd;
  }
  const char* title() override { return "FUD-X3"; }
  void body(App& app, int topY, int bottomY) override;
  void onButton(Btn b, App& app) override;
  void hints(const char* out[6]) override;
  bool wantsLiveRefresh() override { return true; }
  uint32_t timedRefreshMs() override { return 60000; }  // keep the clock current

 private:
  bool alertActive() const;
  Screen* settings_ = nullptr;
  Screen* scans_ = nullptr;
  Detections* det_ = nullptr;
  SdStore* sd_ = nullptr;
};

// A reusable single-choice picker: A/B move, D selects (fires a callback and
// returns), C cancels. Option labels must outlive the screen (static).
class OptionScreen : public Screen {
 public:
  void open(const char* title, const char* const* opts, int n, int cur,
            void (*onPick)(int)) {
    title_ = title;
    opts_ = opts;
    n_ = n;
    sel_ = (cur >= 0 && cur < n) ? cur : 0;
    onPick_ = onPick;
  }
  const char* title() override { return title_; }
  bool showMascot() override { return true; }
  void body(App& app, int topY, int bottomY) override;
  void onButton(Btn b, App& app) override;
  void hints(const char* out[6]) override;

 private:
  const char* title_ = "";
  const char* const* opts_ = nullptr;
  int n_ = 0;
  int sel_ = 0;
  void (*onPick_)(int) = nullptr;
};

// A full-screen one-line message (for not-yet-built settings items).
class InfoScreen : public Screen {
 public:
  void open(const char* title, const char* msg) {
    title_ = title;
    msg_ = msg;
  }
  const char* title() override { return title_; }
  bool showMascot() override { return true; }
  void body(App& app, int topY, int bottomY) override;
  void onButton(Btn b, App& app) override;
  void hints(const char* out[6]) override;

 private:
  const char* title_ = "";
  const char* msg_ = "";
};

// About: version, source link, license, and open-source disclosures.
class AboutScreen : public Screen {
 public:
  const char* title() override { return "About"; }
  void body(App& app, int topY, int bottomY) override;
  void onButton(Btn b, App& app) override;
  void hints(const char* out[6]) override;
};

// Set the RTC by hand: A/B adjust the active field, D moves to the next field,
// C saves. Hold-A (home) cancels without saving.
class SetClockScreen : public Screen {
 public:
  void link(Clock* clk) { clk_ = clk; }
  void open();
  const char* title() override { return "Set Clock"; }
  bool showMascot() override { return true; }
  void body(App& app, int topY, int bottomY) override;
  void onButton(Btn b, App& app) override;
  void hints(const char* out[6]) override;

 private:
  void adjust(int delta);
  Clock* clk_ = nullptr;
  RtcTime t_ = {2026, 1, 1, 0, 0, 0};
  int field_ = 0;  // 0=year 1=month 2=day 3=hour 4=min
};

// SD card status: present/absent, log file count + size, today's log file.
class SdCardScreen : public Screen {
 public:
  void link(SdStore* sd, Clock* clk) {
    sd_ = sd;
    clk_ = clk;
  }
  const char* title() override { return "SD Card"; }
  bool showMascot() override { return true; }
  void body(App& app, int topY, int bottomY) override;
  void onButton(Btn b, App& app) override;
  void hints(const char* out[6]) override;
  bool wantsLiveRefresh() override { return true; }

 private:
  SdStore* sd_ = nullptr;
  Clock* clk_ = nullptr;
};

// Delete log files, scoped Today / This Week / All, with a confirm step.
class DeleteLogsScreen : public Screen {
 public:
  void link(SdStore* sd, Clock* clk) {
    sd_ = sd;
    clk_ = clk;
  }
  void open();
  const char* title() override { return "Delete Logs"; }
  bool showMascot() override { return true; }
  void body(App& app, int topY, int bottomY) override;
  void onButton(Btn b, App& app) override;
  void hints(const char* out[6]) override;

 private:
  int doDelete(int kind);
  SdStore* sd_ = nullptr;
  Clock* clk_ = nullptr;
  int sel_ = 0;
  int scopeN_ = 0;
  const char* scopeName_[3] = {nullptr, nullptr, nullptr};
  int scopeKind_[3] = {0, 0, 0};  // 0=Today 1=Week 2=All
  bool confirming_ = false;
  bool done_ = false;
  int deleted_ = 0;
};

// Manage the ignore list: view entries, Remove to drop one.
class IgnoreListScreen : public Screen {
 public:
  void open() { sel_ = 0; }
  const char* title() override { return "Ignore List"; }
  void body(App& app, int topY, int bottomY) override;
  void onButton(Btn b, App& app) override;
  void hints(const char* out[6]) override;

 private:
  int sel_ = 0;
};

// A battery drain test: unplug to auto-start a stopwatch that logs %/mV/mA to
// SD every minute until the device dies (e-ink holds the final screen).
// Scanning keeps running; alerts don't interrupt it.
class BatteryTestScreen : public Screen {
 public:
  void link(SdStore* sd, Clock* clk) {
    sd_ = sd;
    clk_ = clk;
  }
  void open() { started_ = false; }
  const char* title() override { return "Battery Test"; }
  void body(App& app, int topY, int bottomY) override;
  void onButton(Btn b, App& app) override;
  void hints(const char* out[6]) override;
  // Poll fast while waiting so the unplug is caught within seconds; once the
  // test is running, drop to a minute cadence so the panel's own draw doesn't
  // skew the measurement.
  uint32_t timedRefreshMs() override { return started_ ? 60000 : 2000; }
  bool inhibitSleep() override { return started_; }  // run until it truly dies

 private:
  void logRow(uint32_t elapsedS);
  SdStore* sd_ = nullptr;
  Clock* clk_ = nullptr;
  bool started_ = false;
  uint32_t startMs_ = 0;
  uint32_t lastLogMs_ = 0;
  uint8_t startPct_ = 0;
};

class SettingsScreen : public ListScreen {
 public:
  void link(SetClockScreen* setClock, SdCardScreen* sdCard,
            DeleteLogsScreen* deleteLogs, OptionScreen* option,
            IgnoreListScreen* ignoreList, InfoScreen* info, Detections* det,
            BatteryTestScreen* batteryTest, AboutScreen* about) {
    setClock_ = setClock;
    sdCard_ = sdCard;
    deleteLogs_ = deleteLogs;
    option_ = option;
    ignoreList_ = ignoreList;
    info_ = info;
    det_ = det;
    batteryTest_ = batteryTest;
    about_ = about;
  }
  const char* title() override { return "Settings"; }
  bool showMascot() override { return true; }

 protected:
  int count() override;
  void label(int i, char* buf, size_t n) override;
  void onSelect(int i, App& app) override;

 private:
  SetClockScreen* setClock_ = nullptr;
  SdCardScreen* sdCard_ = nullptr;
  DeleteLogsScreen* deleteLogs_ = nullptr;
  OptionScreen* option_ = nullptr;
  IgnoreListScreen* ignoreList_ = nullptr;
  InfoScreen* info_ = nullptr;
  Detections* det_ = nullptr;
  BatteryTestScreen* batteryTest_ = nullptr;
  AboutScreen* about_ = nullptr;
};

// The informational live Wi-Fi list (SSID + signal), the sole ex-"Toy". Shows
// what the last passive scan saw; refreshes each Wi-Fi window. No action on a
// row — it's just a view.
class WifiNetScreen : public ListScreen {
 public:
  const char* title() override { return "Wi-Fi Networks"; }
  bool wantsLiveRefresh() override { return true; }
  void body(App& app, int topY, int bottomY) override;

 protected:
  int count() override;
  void label(int i, char* buf, size_t n) override;

 private:
  WifiAp aps_[wifiaps::MAXN];
  int n_ = 0;
};

class CategoryScreen;

class ScanSummaryScreen : public ListScreen {
 public:
  void link(Detections* det, CategoryScreen* cat, WifiNetScreen* wifi) {
    det_ = det;
    cat_ = cat;
    wifi_ = wifi;
  }
  const char* title() override { return "Scan Summaries"; }
  bool showMascot() override { return true; }
  bool wantsLiveRefresh() override { return true; }

 protected:
  int count() override;
  void label(int i, char* buf, size_t n) override;
  void onSelect(int i, App& app) override;

 private:
  Detections* det_ = nullptr;
  CategoryScreen* cat_ = nullptr;
  WifiNetScreen* wifi_ = nullptr;
};

class DetailScreen;

// One category's live device list (e.g. all Trackers). Drills into a detail.
class CategoryScreen : public ListScreen {
 public:
  void link(Detections* det, DetailScreen* detail) {
    det_ = det;
    detail_ = detail;
  }
  void open(Cat c) {
    cat_ = c;
    index_ = 0;
    top_ = 0;
  }
  const char* title() override { return catName(cat_); }
  bool wantsLiveRefresh() override { return true; }

 protected:
  int count() override;
  void label(int i, char* buf, size_t n) override;
  void onSelect(int i, App& app) override;

 private:
  static constexpr int MAXROWS = 64;
  Detections* det_ = nullptr;
  DetailScreen* detail_ = nullptr;
  Cat cat_ = Cat::None;
  DetView views_[MAXROWS];   // snapshot for display order (Open Wi-Fi sorts by name)
  int slot_[MAXROWS] = {0};  // original store index, so detail opens the right one
  int n_ = 0;
};

// One device, all the detail we have. NEXT/PREV step to sibling devices.
class DetailScreen : public Screen {
 public:
  void link(Detections* det) { det_ = det; }
  void open(Cat c, int index) {
    cat_ = c;
    index_ = index;
  }
  const char* title() override { return "Detail"; }
  void body(App& app, int topY, int bottomY) override;
  void onButton(Btn b, App& app) override;
  void hints(const char* out[6]) override;
  bool wantsLiveRefresh() override { return true; }

 private:
  Detections* det_ = nullptr;
  Cat cat_ = Cat::None;
  int index_ = 0;
};
