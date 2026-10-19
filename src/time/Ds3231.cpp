#include "Ds3231.h"
#include <Arduino.h>
#include <Wire.h>

static constexpr uint8_t ADDR = 0x68;
static constexpr int PIN_SDA = 20;
static constexpr int PIN_SCL = 0;
static constexpr uint8_t REG_TIME = 0x00;
static constexpr uint8_t REG_STATUS = 0x0F;
static constexpr uint8_t OSF = 0x80;  // oscillator-stop flag (set = time lost)

static uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
static uint8_t dec2bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }

static bool readReg(uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)ADDR, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; ++i) buf[i] = Wire.read();
  return true;
}

static bool writeReg(uint8_t reg, const uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  for (uint8_t i = 0; i < n; ++i) Wire.write(buf[i]);
  return Wire.endTransmission() == 0;
}

bool Ds3231::begin() {
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.beginTransmission(ADDR);
  present_ = (Wire.endTransmission() == 0);
  return present_;
}

bool Ds3231::valid() {
  if (!present_) return false;
  uint8_t st;
  if (!readReg(REG_STATUS, &st, 1)) return false;
  return (st & OSF) == 0;  // OSF clear => the clock has been set and kept time
}

bool Ds3231::read(RtcTime& t) {
  if (!present_) return false;
  uint8_t r[7];
  if (!readReg(REG_TIME, r, 7)) return false;
  t.sec = bcd2dec(r[0] & 0x7F);
  t.min = bcd2dec(r[1] & 0x7F);
  t.hour = bcd2dec(r[2] & 0x3F);          // 24-hour
  t.day = bcd2dec(r[4] & 0x3F);
  t.mon = bcd2dec(r[5] & 0x1F);           // bit7 = century, ignored
  t.year = 2000 + bcd2dec(r[6]);
  return true;
}

bool Ds3231::set(const RtcTime& t) {
  if (!present_) return false;
  uint8_t r[7];
  r[0] = dec2bcd(t.sec);
  r[1] = dec2bcd(t.min);
  r[2] = dec2bcd(t.hour);                 // 24-hour (bit6 = 0)
  r[3] = 1;                               // day-of-week unused
  r[4] = dec2bcd(t.day);
  r[5] = dec2bcd(t.mon);
  r[6] = dec2bcd((uint8_t)(t.year % 100));
  if (!writeReg(REG_TIME, r, 7)) return false;
  // Clear the oscillator-stop flag so valid() reports the time as good.
  uint8_t st;
  if (readReg(REG_STATUS, &st, 1)) {
    st &= ~OSF;
    writeReg(REG_STATUS, &st, 1);
  }
  return true;
}
