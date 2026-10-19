// A classified detection, passed from the scanner task to the UI task via a
// queue. `label` points at a static string (a signature label or a literal),
// so storing the pointer is safe.
#pragma once
#include <stdint.h>
#include "Category.h"

struct ScanEvent {
  uint8_t key[6];     // device address (or rotating MAC for AirTags)
  Cat cat;
  const char* label;  // static category/brand label
  char name[24];      // the device's advertised name / SSID ("" if none)
  int8_t rssi;
  char conf;          // 'H' / 'M' / 'L'
};
