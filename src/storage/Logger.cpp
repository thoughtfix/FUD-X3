#include "Logger.h"
#include <Arduino.h>
#include <stdio.h>
#include "../Config.h"
#include "../scan/Category.h"
#include "../time/Clock.h"
#include "SdStore.h"

static const char* kDir = "/.fudx3";
static const char* kHeader = "timestamp,category,label,name,mac,rssi,conf";

void Logger::begin(SdStore* sd, Clock* clk) {
  sd_ = sd;
  clk_ = clk;
}

void Logger::onMount() {
  if (sd_) sd_->ensureDir(kDir);
}

void Logger::currentPath(char* buf, size_t n) {
  char ds[12];
  if (clk_ && clk_->dateStamp(ds, sizeof(ds)))
    snprintf(buf, n, "%s/%s.csv", kDir, ds);
  else
    snprintf(buf, n, "%s/uptime.csv", kDir);  // clock unset: one fallback file
}

void Logger::log(const ScanEvent& e, uint32_t now) {
  if (!config.logging) return;          // logging disabled in settings
  if (!sd_ || !sd_->present()) return;  // no card: disregard, no buffering

  char path[40];
  currentPath(path, sizeof(path));
  if (!sd_->exists(path)) {
    sd_->ensureDir(kDir);
    sd_->appendLine(path, kHeader);  // first write of a new dated file
  }

  char ts[24];
  if (!clk_ || !clk_->stamp(ts, sizeof(ts)))
    snprintf(ts, sizeof(ts), "+%lus", (unsigned long)(now / 1000));

  char mac[20];
  snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", e.key[0], e.key[1],
           e.key[2], e.key[3], e.key[4], e.key[5]);
  char line[160];
  snprintf(line, sizeof(line), "%s,%s,%s,%s,%s,%d,%c", ts, catName(e.cat),
           e.label, e.name[0] ? e.name : "", mac, e.rssi, e.conf);
  sd_->appendLine(path, line);
}
