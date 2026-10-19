// BleScanner: one BLE scan window, classified into the ScanBus. No task of its
// own — the ScanCoordinator drives it so the radio is only ever touched from
// one place (the WiFi window runs with BLE idle).
#pragma once
#include <stdint.h>

class BleScanner {
 public:
  void start();                  // NimBLEDevice::init + scan params
  void stop();                   // NimBLEDevice::deinit (frees BT memory)
  void scanWindow(uint32_t ms);  // collect for ms, classify, enqueue
};
