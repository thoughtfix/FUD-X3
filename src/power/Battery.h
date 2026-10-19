// Battery: the app-facing snapshot of the X3's fuel gauge.
//
// The X3 carries a TI BQ27220 fuel gauge on the same I2C bus as the RTC
// (SDA=GPIO20, SCL=GPIO0). A single global snapshot is refreshed by Fuel::tick()
// on the main loop; screens and the title-bar battery icon read it directly so
// nothing has to thread a Fuel reference through the whole UI.
#pragma once
#include <stdint.h>

struct BatteryState {
  bool present = false;   // the fuel gauge answered on I2C
  bool valid = false;     // at least one good reading has landed
  uint8_t pct = 0;        // state of charge, 0-100
  uint16_t mv = 0;        // pack voltage, millivolts
  int16_t ma = 0;         // current, signed; <0 discharging, >0 charging
};

extern BatteryState gBatt;

// On battery (not charging) and down in the warning band.
inline bool battLow() {
  return gBatt.present && gBatt.valid && gBatt.ma < 0 && gBatt.pct <= 15;
}
// On battery and critically low — time to sleep gracefully.
inline bool battCritical() {
  return gBatt.present && gBatt.valid && gBatt.ma < 0 && gBatt.pct <= 5;
}
