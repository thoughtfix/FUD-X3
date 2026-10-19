#include "Gfx.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <stdio.h>
#include <string.h>

namespace gfx {

static const GFXfont* fontFor(int s) {
  if (s <= 2) return &FreeSans9pt7b;        // hints / small
  if (s == 3) return &FreeSans12pt7b;       // content
  if (s == 4) return &FreeSans18pt7b;       // list rows
  if (s == 7) return &FreeSansBold18pt7b;   // titles (bold)
  return &FreeSansBold24pt7b;               // 5/6: big
}

int lineH(int s) { return fontFor(s)->yAdvance; }

// Uniform ascent for a tier (measured from a tall sample), so lines of mixed
// text align on the same top edge.
static int ascentOf(Adafruit_GFX& c, int s) {
  c.setFont(fontFor(s));
  int16_t x1, y1;
  uint16_t w, h;
  c.getTextBounds("Ag", 0, 0, &x1, &y1, &w, &h);
  return -y1;
}

int textW(Adafruit_GFX& c, const char* t, int s) {
  c.setFont(fontFor(s));
  int16_t x1, y1;
  uint16_t w, h;
  c.getTextBounds(t, 0, 0, &x1, &y1, &w, &h);
  return (int)w;
}

static int widthOf(Adafruit_GFX& c, const char* s) {
  int16_t x1, y1;
  uint16_t w, h;
  c.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return (int)w;
}

static void drawTop(Adafruit_GFX& c, int x, int topY, const char* s, int size,
                    uint16_t color) {
  c.setFont(fontFor(size));
  c.setTextColor(color);
  c.setCursor(x, topY + ascentOf(c, size));
  c.print(s);
}

int clippedLine(Adafruit_GFX& c, int x, int y, int maxX, const char* text,
                int size, uint16_t color) {
  c.setFont(fontFor(size));
  const int avail = maxX - x;
  char buf[192];
  strncpy(buf, text, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  int len = (int)strlen(buf);
  while (len > 0) {
    buf[len] = 0;
    if (widthOf(c, buf) <= avail) break;
    --len;
  }
  drawTop(c, x, y, buf, size, color);
  return y + lineH(size);
}

int wrapText(Adafruit_GFX& c, int x, int y, int maxX, int maxY, const char* text,
             int size, uint16_t color) {
  c.setFont(fontFor(size));
  const int lh = lineH(size);
  const int avail = maxX - x;
  char line[192];
  line[0] = 0;
  int lineLen = 0;
  const char* p = text;

  while (*p) {
    while (*p == ' ') ++p;
    if (!*p) break;
    const char* e = p;
    while (*e && *e != ' ') ++e;
    int wl = (int)(e - p);
    if (wl > 180) wl = 180;
    char word[192];
    memcpy(word, p, wl);
    word[wl] = 0;

    char cand[256];
    if (lineLen)
      snprintf(cand, sizeof(cand), "%s %s", line, word);
    else
      snprintf(cand, sizeof(cand), "%s", word);

    if (widthOf(c, cand) <= avail) {
      strncpy(line, cand, sizeof(line) - 1);
      line[sizeof(line) - 1] = 0;
      lineLen = (int)strlen(line);
      p = e;
    } else if (lineLen == 0) {
      // Single word too wide: truncate it onto its own line.
      int len = wl;
      while (len > 0) {
        word[len] = 0;
        if (widthOf(c, word) <= avail) break;
        --len;
      }
      if (y + lh > maxY) return y;
      drawTop(c, x, y, word, size, color);
      y += lh;
      p = e;
    } else {
      if (y + lh > maxY) return y;
      drawTop(c, x, y, line, size, color);
      y += lh;
      line[0] = 0;
      lineLen = 0;
      // leave p; re-handle this word on the new line
    }
  }
  if (lineLen && y + lh <= maxY) {
    drawTop(c, x, y, line, size, color);
    y += lh;
  }
  return y;
}

}  // namespace gfx
