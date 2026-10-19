// Input: a semantic button layer over the SDK's raw ADC button state.
//
// The X3's physical controls map to raw bits (reverse-engineered on device):
//   A=0x01  B=0x02  C=0x04  D=0x08  Prev(left side)=0x10  Next(right side)=0x20
//   Power=0x40
// We expose meaning, not those bits, and resolve taps on release so a long hold
// of A can mean "home" instead of a tap. Taps are latched so a press is never
// lost across a blocking e-ink refresh (which stalls the loop for ~380ms).
#pragma once
#include <stdint.h>
#include <InputManager.h>

enum class Btn : uint8_t { None, A, B, C, D, Prev, Next, Power };

const char* btnName(Btn b);

class Input {
 public:
  void begin();
  void poll();  // call every loop iteration

  Btn takePress();        // consume the last tap (None if none pending)
  bool takeHomeGesture(); // true once after a long hold of A

 private:
  InputManager im_;
  uint8_t last_ = 0;
  unsigned long downAt_[7] = {0};  // press time per bit index 0..6
  Btn pending_ = Btn::None;
  bool homePending_ = false;
};
