// ScanCoordinator: owns the single scan task and the radio rotation. Only one
// 2.4 GHz radio is live at a time (BLE window with WiFi off, then a WiFi window
// with BLE idle), which is both the time-slicing and the memory/stability
// strategy on this no-PSRAM board.
#pragma once

class ScanCoordinator {
 public:
  void begin();
  // 'B' = BLE window, 'W' = WiFi window, '-' = starting up.
  char activeRadio() const;
};
