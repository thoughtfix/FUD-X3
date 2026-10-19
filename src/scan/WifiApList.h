// WifiApList: a small snapshot of the Wi-Fi networks seen in the last scan,
// for the informational "Wi-Fi Networks" view. The scan task publishes the
// whole list at the end of each AP scan; the UI task takes a sorted snapshot.
// This is display-only and never transmits — it's just what WiFi.scanNetworks
// already saw, deduplicated by SSID.
#pragma once
#include <stdint.h>

struct WifiAp {
  char ssid[33];
  int8_t rssi;
  bool open;
};

namespace wifiaps {

static constexpr int MAXN = 28;  // plenty for a room; bounds the RAM/stack cost

void begin();                            // create the mutex
void publish(const WifiAp* aps, int n);  // replace the live list (scan task)
// Copy the live list into out (up to maxOut), sorted strongest-first. Returns
// the number copied.
int snapshot(WifiAp* out, int maxOut);

}  // namespace wifiaps
