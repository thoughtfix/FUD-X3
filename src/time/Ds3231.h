// Minimal DS3231 RTC driver over I2C (addr 0x68), written from the public
// register map (BCD time registers 0x00-0x06, status 0x0F). The X3 wires the
// DS3231 on SDA=GPIO20, SCL=GPIO0. We keep and read 24-hour local time
// directly (no timezone math) since the user sets wall-clock time by hand.
#pragma once
#include <stdint.h>

struct RtcTime {
  uint16_t year;  // full year, e.g. 2026
  uint8_t mon;    // 1-12
  uint8_t day;    // 1-31
  uint8_t hour;   // 0-23
  uint8_t min;    // 0-59
  uint8_t sec;    // 0-59
};

class Ds3231 {
 public:
  bool begin();                 // Wire.begin(SDA,SCL) + probe the chip
  bool present() const { return present_; }
  bool valid();                 // true once a time has been set (OSF clear)
  bool read(RtcTime& t);
  bool set(const RtcTime& t);   // write time and clear the oscillator-stop flag

 private:
  bool present_ = false;
};
