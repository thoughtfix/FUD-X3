#include "Input.h"
#include <Arduino.h>

static constexpr unsigned long HOLD_MS = 600;  // A held this long = home gesture

// Bit index (0..6) -> semantic button.
static Btn btnForBit(int bit) {
  switch (bit) {
    case 0: return Btn::A;
    case 1: return Btn::B;
    case 2: return Btn::C;
    case 3: return Btn::D;
    case 4: return Btn::Prev;
    case 5: return Btn::Next;
    case 6: return Btn::Power;
  }
  return Btn::None;
}

const char* btnName(Btn b) {
  switch (b) {
    case Btn::A: return "A";
    case Btn::B: return "B";
    case Btn::C: return "C";
    case Btn::D: return "D";
    case Btn::Prev: return "PREV";
    case Btn::Next: return "NEXT";
    case Btn::Power: return "POWER";
    default: return "-";
  }
}

void Input::begin() { im_.begin(); }

void Input::poll() {
  im_.update();
  const uint8_t raw = im_.getState();
  const unsigned long now = millis();

  for (int bit = 0; bit < 7; ++bit) {
    const uint8_t mask = 1 << bit;
    const bool nowDown = raw & mask;
    const bool wasDown = last_ & mask;

    if (nowDown && !wasDown) {
      downAt_[bit] = now;  // press edge
    } else if (!nowDown && wasDown) {
      // release edge: decide tap vs. home
      const unsigned long dur = now - downAt_[bit];
      if (bit == 0 && dur >= HOLD_MS) {
        homePending_ = true;  // long A = home, not a tap
      } else {
        pending_ = btnForBit(bit);  // latch the tap
      }
    }
  }
  last_ = raw;
}

Btn Input::takePress() {
  const Btn b = pending_;
  pending_ = Btn::None;
  return b;
}

bool Input::takeHomeGesture() {
  const bool h = homePending_;
  homePending_ = false;
  return h;
}
