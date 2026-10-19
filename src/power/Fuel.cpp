#include "Fuel.h"
#include <Arduino.h>
#include <Wire.h>

BatteryState gBatt;

static constexpr uint8_t ADDR = 0x55;
static constexpr uint8_t REG_VOLT = 0x08;  // Voltage(), mV
static constexpr uint8_t REG_CUR = 0x0C;   // Current(), signed mA
static constexpr uint8_t REG_SOC = 0x2C;   // StateOfCharge(), %
static constexpr uint32_t READ_MS = 5000;  // the gauge moves slowly; poll rarely

// Read one little-endian 16-bit command word. Returns false on any bus fault.
static bool rd16(uint8_t reg, uint16_t& out) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)ADDR, 2) != 2) return false;
  uint8_t lo = Wire.read();
  uint8_t hi = Wire.read();
  out = (uint16_t)lo | ((uint16_t)hi << 8);
  return true;
}

bool Fuel::begin() {
  Wire.beginTransmission(ADDR);
  present_ = (Wire.endTransmission() == 0);
  gBatt.present = present_;
  if (present_) read();
  return present_;
}

bool Fuel::read() {
  if (!present_) return false;
  uint16_t soc = 0, mv = 0, raw = 0;
  bool ok = rd16(REG_SOC, soc) && rd16(REG_VOLT, mv) && rd16(REG_CUR, raw);
  if (!ok) return false;
  gBatt.pct = soc > 100 ? 100 : (uint8_t)soc;
  gBatt.mv = mv;
  gBatt.ma = (int16_t)raw;
  gBatt.valid = true;
  return true;
}

void Fuel::tick(uint32_t now) {
  if (!present_) return;
  if (gBatt.valid && now - lastRead_ < READ_MS) return;
  lastRead_ = now;
  read();
}
