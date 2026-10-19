// WifiScanner: a passive Wi-Fi window. An AP scan classifies networks against
// the SSID/OUI signatures (plus evil-twin and the "pwned" beacon), and a
// promiscuous pass counts deauth/disassoc frames to flag a flood. Receive only
// — nothing is ever transmitted.
//
// Driven by the ScanCoordinator with the radio in STA mode and BLE idle.
#pragma once
#include <stdint.h>

class WifiScanner {
 public:
  void scanAps();                         // station scan + classify
  void monitorDeauth(uint32_t perChanMs); // promiscuous hop 1/6/11, detect flood
};
