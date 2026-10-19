// Gfx: text helpers over Adafruit GFX FreeFonts (proportional, anti-blocky).
// Size "tiers" map to concrete fonts (see Gfx.cpp); callers pass a tier number.
// All drawing goes through here so content word-wraps and clips instead of
// colliding, and so the whole UI changes font in one place.
#pragma once
#include <Adafruit_GFX.h>

namespace gfx {

// Tiers: 2=small/hints, 3=content, 4=rows, 5/6=big, 7=title (bold).
int lineH(int size);                               // line advance for a tier
int textW(Adafruit_GFX& c, const char* text, int size);  // rendered width

// Word-wrap text in [x,maxX) from y, stopping before maxY. Returns next y.
int wrapText(Adafruit_GFX& c, int x, int y, int maxX, int maxY, const char* text,
             int size, uint16_t color);
// One line, truncated to fit [x,maxX) with no overrun. Returns next y.
int clippedLine(Adafruit_GFX& c, int x, int y, int maxX, const char* text,
                int size, uint16_t color);

}  // namespace gfx
