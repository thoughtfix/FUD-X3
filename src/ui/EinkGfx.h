// EinkGfx: an Adafruit_GFX drawing surface that writes straight into the
// EInkDisplay's own 1-bit framebuffer, so we don't keep a second ~52KB canvas
// in heap (this board has no PSRAM). Bit layout matches GFXcanvas1 exactly
// (row-major, MSB-first, set bit = color 1 = white on this panel), and the
// rotation transform mirrors GFXcanvas1 so portrait drawing lands correctly in
// the native-orientation buffer.
#pragma once
#include <Adafruit_GFX.h>
#include <string.h>

class EinkGfx : public Adafruit_GFX {
 public:
  EinkGfx(int16_t nativeW, int16_t nativeH)
      : Adafruit_GFX(nativeW, nativeH), nw_(nativeW), nh_(nativeH) {}

  void attach(uint8_t* buffer) { buf_ = buffer; }

  void fillScreen(uint16_t color) override {
    if (buf_) memset(buf_, color ? 0xFF : 0x00, (size_t)((nw_ + 7) / 8) * nh_);
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (!buf_) return;
    if (x < 0 || y < 0 || x >= width() || y >= height()) return;
    // Map rotated coords back to the native buffer (same cases as GFXcanvas1).
    int16_t t;
    switch (getRotation()) {
      case 1: t = x; x = nw_ - 1 - y; y = t; break;
      case 2: x = nw_ - 1 - x; y = nh_ - 1 - y; break;
      case 3: t = x; x = y; y = nh_ - 1 - t; break;
    }
    uint8_t* p = &buf_[(x >> 3) + (size_t)y * ((nw_ + 7) / 8)];
    const uint8_t mask = 0x80 >> (x & 7);
    if (color) *p |= mask;
    else *p &= ~mask;
  }

 private:
  uint8_t* buf_ = nullptr;
  const int16_t nw_;
  const int16_t nh_;
};
