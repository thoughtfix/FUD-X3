#include "App.h"
#include "Gfx.h"
#include "../art/ZuseFaces.h"
#include "../power/Battery.h"
#include "../Config.h"
#include <stdio.h>
#include <string.h>

static constexpr int PAD = 10;
static constexpr int TITLE_SIZE = 7;  // bold title tier
static constexpr int HINT_SIZE = 2;
static int titleH() { return gfx::lineH(TITLE_SIZE) + 2 * PAD; }  // top band
static int hintH() { return gfx::lineH(HINT_SIZE) + 2 * PAD + 6; }  // bottom band

// The front buttons are two rockers, [A B] then a gap then [C D], so the tabs
// are grouped the same way instead of being spread in even quarters.
static constexpr int CENTER_GAP = 44;  // blank space between the two pairs

// Screen left-to-right order of the four front tabs, as hint indices (0=A..3=D).
// If the tabs don't sit above the matching physical buttons, flip this.
static const int FRONT_ORDER[4] = {0, 1, 2, 3};

// A small battery gauge for the top-right of the title bar. Outline + a fill
// proportional to charge, a terminal nub, and a bolt overlay while charging.
// Drawn only when the fuel gauge is present.
static void drawBattery(Adafruit_GFX& c, int rightX, int midY) {
  if (!gBatt.present || !gBatt.valid) return;
  const int bw = 46, bh = 24, nub = 5;
  const int x = rightX - bw - nub;
  const int y = midY - bh / 2;
  c.drawRect(x, y, bw, bh, FUD_BLACK);
  c.drawRect(x + 1, y + 1, bw - 2, bh - 2, FUD_BLACK);  // 2px wall, crisp on e-ink
  c.fillRect(x + bw, y + bh / 2 - nub, nub, nub * 2, FUD_BLACK);  // + terminal
  const int innerW = bw - 8;
  int fillW = (innerW * gBatt.pct) / 100;
  if (fillW < 2 && gBatt.pct > 0) fillW = 2;
  if (fillW > innerW) fillW = innerW;
  if (fillW > 0) c.fillRect(x + 4, y + 4, fillW, bh - 8, FUD_BLACK);
  if (gBatt.ma > 0) {  // charging: a bolt notch punched through the fill
    const int cx = x + bw / 2, cy = y + bh / 2;
    c.fillTriangle(cx - 1, y + 3, cx - 7, cy + 1, cx + 1, cy + 1, FUD_WHITE);
    c.fillTriangle(cx + 1, y + bh - 3, cx + 7, cy - 1, cx - 1, cy - 1, FUD_WHITE);
  }
}

void App::setHome(Screen* s) {
  home_ = s;
  sp_ = 0;
  stack_[sp_++] = s;
  dirty_ = true;
  next_ = Refresh::Full;
}

void App::go(Screen* s) {
  if (sp_ < (int)(sizeof(stack_) / sizeof(stack_[0]))) stack_[sp_++] = s;
  dirty_ = true;
  next_ = Refresh::Half;
}

void App::back() {
  if (sp_ > 1) --sp_;
  dirty_ = true;
  next_ = Refresh::Half;
}

void App::home() {
  sp_ = 0;
  if (home_) stack_[sp_++] = home_;
  dirty_ = true;
  next_ = Refresh::Half;
}

// Center a single clipped line within [boxX, boxX+boxW).
static void centerLine(Adafruit_GFX& c, int boxX, int boxW, int y,
                       const char* text, int size, uint16_t color) {
  int tw = gfx::textW(c, text, size);
  if (tw > boxW - 6) tw = boxW - 6;
  int x = boxX + (boxW - tw) / 2;
  if (x < boxX + 3) x = boxX + 3;
  gfx::clippedLine(c, x, y, boxX + boxW - 3, text, size, color);
}

// Bottom chrome: four tabs grouped as two pairs ([A B] gap [C D]) so each label
// sits above its physical button. Side buttons aren't labeled here for now (on
// current screens they duplicate the front tabs or are inactive). Inactive
// buttons leave an empty tab rather than a mislabeled one.
void App::drawHints() {
  Screen* s = cur();
  if (!s) return;
  const char* h[6] = {"", "", "", "", "", ""};
  s->hints(h);  // A,B,C,D,Prev,Next

  Adafruit_GFX& c = d_.cv();
  const int barTop = FUD_H - hintH();
  c.drawFastHLine(0, barTop, FUD_W, FUD_BLACK);

  // Each rocker pair's outer tab (A on the left pair, D on the right) is wider
  // than its inner tab (B, C), so the edge buttons read bigger.
  const int groupW = (FUD_W - CENTER_GAP) / 2;  // width of each [x x] pair
  const int innerW = groupW * 100 / 247;        // outer = 1.47 * inner
  const int outerW = groupW - innerW;
  const int tabX[4] = {0, outerW, groupW + CENTER_GAP,
                       groupW + CENTER_GAP + innerW};
  const int tabW[4] = {outerW, innerW, innerW, outerW};
  const int labelY = barTop + PAD;

  // Dividers inside each pair (not across the center gap).
  c.drawFastVLine(tabX[1], barTop, hintH(), FUD_BLACK);
  c.drawFastVLine(tabX[3], barTop, hintH(), FUD_BLACK);

  for (int i = 0; i < 4; ++i) {
    const char* label = h[FRONT_ORDER[i]];
    if (label[0]) centerLine(c, tabX[i], tabW[i], labelY, label, HINT_SIZE,
                             FUD_BLACK);
  }
}

void App::render() {
  Screen* s = cur();
  if (!s) return;
  Adafruit_GFX& c = d_.cv();
  d_.clear();

  gfx::clippedLine(c, PAD, PAD, FUD_W - PAD, s->title(), TITLE_SIZE, FUD_BLACK);
  drawBattery(c, FUD_W - PAD, PAD + gfx::lineH(TITLE_SIZE) / 2);
  c.drawFastHLine(0, titleH() - 2, FUD_W, FUD_BLACK);

  s->body(*this, titleH() + PAD, FUD_H - hintH() - PAD);
  // A little Zuse in the bottom-right corner of screens with room for him.
  if (s->showMascot())
    c.drawBitmap(FUD_W - ZUSE_SMALL_W - 10, FUD_H - hintH() - ZUSE_SMALL_H - 8,
                 zuse_small, ZUSE_SMALL_W, ZUSE_SMALL_H, FUD_BLACK);
  drawHints();

  switch (next_) {
    case Refresh::Full:
      d_.pushFull();
      fastCount_ = 0;
      break;
    case Refresh::Half:
      d_.pushHalf();  // clean, no white-wash flash
      fastCount_ = 0;
      break;
    case Refresh::Fast:
      if (++fastCount_ >= 8) {
        d_.pushHalf();  // periodic de-ghost without a full wash
        fastCount_ = 0;
      } else {
        d_.pushFast();
      }
      break;
  }
  next_ = Refresh::Fast;  // in-place redraws default to fast
  dirty_ = false;
}

void App::tick() {
  in_.poll();
  const uint32_t now = millis();
  if (lastActivity_ == 0) lastActivity_ = now;

  if (in_.takeHomeGesture()) {
    lastActivity_ = now;
    home();
  } else {
    const Btn b = in_.takePress();
    if (b != Btn::None) lastActivity_ = now;
    if (b == Btn::Power) {
      if (onPower_) onPower_();  // sleep (handled globally)
    } else if (b != Btn::None && cur()) {
      cur()->onButton(b, *this);
    }
  }

  // Optional auto-sleep: fold into the same power handler after N idle minutes.
  // Default is 0 (never) — the device stays awake collecting data until it dies.
  if (onPower_ && config.sleepTimeoutMin > 0 && !(cur() && cur()->inhibitSleep()) &&
      now - lastActivity_ > (uint32_t)config.sleepTimeoutMin * 60000UL) {
    onPower_();
  }

  if (dirty_) render();
}
