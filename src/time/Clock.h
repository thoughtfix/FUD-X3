// Clock: the app-facing view of the RTC. Reports whether an RTC is present and
// whether it holds a set time, and formats stamps for logging and display.
#pragma once
#include <stddef.h>
#include "Ds3231.h"

class Clock {
 public:
  bool begin() { return rtc_.begin(); }
  bool available() const { return rtc_.present(); }
  bool isSet() { return rtc_.valid(); }

  bool get(RtcTime& t) { return rtc_.read(t); }
  bool set(const RtcTime& t) { return rtc_.set(t); }

  // "YYYYMMDD" for dated log filenames. False if the clock isn't set.
  bool dateStamp(char* buf, size_t n);
  // "YYYY-MM-DD HH:MM:SS" for log rows. False if the clock isn't set.
  bool stamp(char* buf, size_t n);

 private:
  Ds3231 rtc_;
};
