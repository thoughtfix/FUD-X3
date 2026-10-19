#include "Config.h"
#include <Preferences.h>

Config config;

void Config::load() {
  Preferences p;
  p.begin("fudx3", true);
  logging = p.getBool("log", true);
  sleepTimeoutMin = p.getUChar("sleepmin", 0);
  p.end();
}

void Config::save() {
  Preferences p;
  p.begin("fudx3", false);
  p.putBool("log", logging);
  p.putUChar("sleepmin", sleepTimeoutMin);
  p.end();
}
