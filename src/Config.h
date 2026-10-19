// Persistent user settings, stored in NVS (survives reboot).
#pragma once
#include <stdint.h>

struct Config {
  bool logging = true;         // write detections to SD
  uint8_t sleepTimeoutMin = 0;  // auto-sleep after N idle min; 0 = never (default)

  void load();
  void save();
};

extern Config config;
