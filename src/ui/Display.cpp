#include "Display.h"

// XTEINK X3 e-paper SPI pins (EInkDisplay::begin() owns SPI.begin()).
static constexpr int8_t EPD_SCLK = 8;
static constexpr int8_t EPD_MOSI = 10;
static constexpr int8_t EPD_CS = 21;
static constexpr int8_t EPD_DC = 4;
static constexpr int8_t EPD_RST = 5;
static constexpr int8_t EPD_BUSY = 6;

// Rotation 3 puts "up" toward the power/lock button (confirmed on device;
// rotation 1 came out 180 degrees off).
static constexpr uint8_t PORTRAIT_ROTATION = 3;

Display::Display()
    : eink_(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY),
      canvas_(EInkDisplay::X3_DISPLAY_WIDTH, EInkDisplay::X3_DISPLAY_HEIGHT) {}

void Display::begin() {
  eink_.setDisplayX3();
  eink_.begin();
  // Draw straight into the panel's own framebuffer (no second heap canvas).
  canvas_.attach(eink_.getFrameBuffer());
  canvas_.setRotation(PORTRAIT_ROTATION);
  clear();
}

void Display::pushFull() { eink_.displayBuffer(EInkDisplay::FULL_REFRESH); }

void Display::pushHalf() { eink_.displayBuffer(EInkDisplay::HALF_REFRESH); }

void Display::pushFast() { eink_.displayBuffer(EInkDisplay::FAST_REFRESH); }

void Display::screenshot() {
  const uint8_t* b = eink_.getFrameBuffer();
  const uint32_t n = EInkDisplay::X3_BUFFER_SIZE;
  static const char* T =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  Serial.println(F("SHOT_START 792 528"));
  uint32_t i = 0;
  int col = 0;
  while (i < n) {
    uint32_t v = (uint32_t)b[i] << 16;
    int pad = 2;
    if (i + 1 < n) { v |= (uint32_t)b[i + 1] << 8; pad = 1; }
    if (i + 2 < n) { v |= b[i + 2]; pad = 0; }
    char out[4];
    out[0] = T[(v >> 18) & 63];
    out[1] = T[(v >> 12) & 63];
    out[2] = pad >= 2 ? '=' : T[(v >> 6) & 63];
    out[3] = pad >= 1 ? '=' : T[v & 63];
    Serial.write(out, 4);
    i += 3;
    col += 4;
    if (col >= 76) { Serial.println(); col = 0; }
  }
  if (col) Serial.println();
  Serial.println(F("SHOT_END"));
}
