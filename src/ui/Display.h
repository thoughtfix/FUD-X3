// Display: owns the X3 e-ink panel and a drawing surface over its framebuffer.
//
// Screen code draws in portrait coordinates (528 wide x 792 tall). The surface
// writes directly into EInkDisplay's native-orientation framebuffer (rotated),
// so there's no second full-screen buffer in heap.
//
// Panel convention: a set bit is white (clearScreen's 0xFF default is white),
// so FUD_WHITE = 1 and FUD_BLACK = 0.
#pragma once
#include <Adafruit_GFX.h>
#include <EInkDisplay.h>
#include "EinkGfx.h"

static constexpr int16_t FUD_W = 528;  // portrait logical width
static constexpr int16_t FUD_H = 792;  // portrait logical height
static constexpr uint16_t FUD_WHITE = 1;
static constexpr uint16_t FUD_BLACK = 0;

class Display {
 public:
  Display();
  void begin();

  Adafruit_GFX& cv() { return canvas_; }
  int16_t width() const { return FUD_W; }
  int16_t height() const { return FUD_H; }

  void clear() { canvas_.fillScreen(FUD_WHITE); }
  void pushFull();  // slow full wash (~930ms); boot only
  void pushHalf();  // clean, no white-wash flash; use on navigation
  void pushFast();  // quickest (~380ms), can ghost; use on in-place updates
  void screenshot();  // base64-dump the framebuffer to Serial (dev/debug)

 private:
  EInkDisplay eink_;
  EinkGfx canvas_;
};
