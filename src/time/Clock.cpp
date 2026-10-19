#include "Clock.h"
#include <stdio.h>

bool Clock::dateStamp(char* buf, size_t n) {
  if (!isSet()) return false;
  RtcTime t;
  if (!get(t)) return false;
  snprintf(buf, n, "%04u%02u%02u", t.year, t.mon, t.day);
  return true;
}

bool Clock::stamp(char* buf, size_t n) {
  if (!isSet()) return false;
  RtcTime t;
  if (!get(t)) return false;
  snprintf(buf, n, "%04u-%02u-%02u %02u:%02u:%02u", t.year, t.mon, t.day,
           t.hour, t.min, t.sec);
  return true;
}
