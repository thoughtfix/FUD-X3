// App: a tiny screen/navigation framework.
//
// A Screen draws a body and declares what each button does right now (its
// hints). App frames every screen the same way — title bar on top, contextual
// hint bar on the bottom — and owns a back stack. Hold-A always returns home.
#pragma once
#include "Display.h"
#include "Input.h"
#include "../time/Clock.h"

class App;

class Screen {
 public:
  virtual ~Screen() {}
  virtual const char* title() = 0;
  // Draw content in the region [topY, bottomY) using app.display().cv().
  virtual void body(App& app, int topY, int bottomY) = 0;
  virtual void onButton(Btn b, App& app) = 0;
  // Fill labels for A,B,C,D,Prev,Next; "" means the button is inactive/hidden.
  virtual void hints(const char* out[6]) = 0;
  // True for screens showing data that changes on its own (so the loop may
  // refresh them periodically). Default: static screens never auto-refresh.
  virtual bool wantsLiveRefresh() { return false; }
  // True for screens with room for the little corner Zuse (short, fixed layout).
  virtual bool showMascot() { return false; }
  // >0 to be redrawn on a wall-clock cadence (ms) even with no input or new
  // data — e.g. a running stopwatch. 0 (default) means no timed refresh.
  virtual uint32_t timedRefreshMs() { return 0; }
  // True to hold off auto-sleep AND the critical-battery dead-sleep while this
  // screen is up — e.g. a battery drain test that must run until it truly dies.
  virtual bool inhibitSleep() { return false; }
};

class App {
 public:
  App(Display& d, Input& in) : d_(d), in_(in) {}

  Display& display() { return d_; }

  void setHome(Screen* s);
  void go(Screen* s);  // push a screen
  void back();         // pop to the previous screen (or home)
  void home();         // clear the stack, return to home
  void redraw() { dirty_ = true; }

  void setPowerHandler(void (*fn)()) { onPower_ = fn; }
  void setClock(Clock* c) { clk_ = c; }  // for the title-bar clock

  void render();  // draw the current screen now
  void tick();    // poll input, dispatch, redraw if needed
  bool currentWantsLive() {
    Screen* s = cur();
    return s && s->wantsLiveRefresh();
  }
  uint32_t currentTimedRefreshMs() {
    Screen* s = cur();
    return s ? s->timedRefreshMs() : 0;
  }
  Screen* current() { return cur(); }

 private:
  void drawHints();

  Display& d_;
  Input& in_;
  void (*onPower_)() = nullptr;
  Clock* clk_ = nullptr;
  Screen* home_ = nullptr;
  Screen* stack_[8] = {nullptr};
  int sp_ = 0;  // stack_[sp_-1] is current; sp_ >= 1 once home is set
  enum class Refresh { Fast, Half, Full };
  bool dirty_ = true;
  Refresh next_ = Refresh::Full;  // boot gets one clean full wash
  int fastCount_ = 0;             // periodic HALF to clear ghost buildup
  uint32_t lastActivity_ = 0;     // millis of last button press (for auto-sleep)

  Screen* cur() { return sp_ > 0 ? stack_[sp_ - 1] : nullptr; }
};
