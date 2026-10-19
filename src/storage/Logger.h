// Logger: appends each new detection to a dated CSV on the SD card. With the
// RTC set it writes /.fudx3/YYYYMMDD.csv with real timestamps (so Delete Logs
// can scope by Today / This Week / All); without a set clock it falls back to a
// single uptime-stamped file. No card = no logging (silently skipped).
#pragma once
#include <stddef.h>
#include "../scan/ScanTypes.h"

class SdStore;
class Clock;

class Logger {
 public:
  void begin(SdStore* sd, Clock* clk);
  void onMount();                            // ensure the log directory exists
  void log(const ScanEvent& e, uint32_t now);

 private:
  void currentPath(char* buf, size_t n);

  SdStore* sd_ = nullptr;
  Clock* clk_ = nullptr;
};
