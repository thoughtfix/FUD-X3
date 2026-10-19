// Detection categories. These are the buckets the Scan Summaries screen counts.
// Ported from FUD's Cat enum, minus sub-GHz (the X3 has no such radio).
#pragma once
#include <stdint.h>

enum class Cat : uint8_t {
  None = 0,
  Tracker,
  Camera,
  Drone,
  HackDev,
  Attack,
  OpenWifi,  // informational (logged, not an alert)
  COUNT,
};

inline const char* catName(Cat c) {
  switch (c) {
    case Cat::Tracker: return "Trackers";
    case Cat::Camera: return "Cameras";
    case Cat::Drone: return "Drones";
    case Cat::HackDev: return "Hacker Devices";
    case Cat::Attack: return "Attacks";
    case Cat::OpenWifi: return "Open Wi-Fi";
    default: return "";
  }
}
