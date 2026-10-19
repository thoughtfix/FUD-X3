// Fuel: minimal driver for the TI BQ27220 fuel gauge over I2C (addr 0x55).
//
// Written from the BQ27220 technical reference: standard command codes are
// read as little-endian 16-bit words -- 0x08 Voltage (mV), 0x0C Current
// (signed mA), 0x2C StateOfCharge (%). The X3 wiring (BQ27220 present on the
// RTC's I2C bus at 0x55) was identified from the crosspoint-reader / FreeInk
// SDK HAL; no code was copied. The X4 variant instead reads battery via ADC --
// the X3 does not, which is why an ADC approach found no free pin.
#pragma once
#include "Battery.h"

class Fuel {
 public:
  bool begin();            // probe 0x55 (Wire must already be begun)
  void tick(uint32_t now); // refresh gBatt at a slow cadence
  bool present() const { return present_; }
  bool read();             // one immediate read into gBatt; true on success

 private:
  bool present_ = false;
  uint32_t lastRead_ = 0;
};
