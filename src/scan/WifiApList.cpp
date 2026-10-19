#include "WifiApList.h"
#include <Arduino.h>
#include <string.h>

namespace wifiaps {

static SemaphoreHandle_t mtx = nullptr;
static WifiAp g_live[MAXN];
static int g_n = 0;

struct Lock {
  Lock() { if (mtx) xSemaphoreTake(mtx, portMAX_DELAY); }
  ~Lock() { if (mtx) xSemaphoreGive(mtx); }
};

void begin() {
  if (!mtx) mtx = xSemaphoreCreateMutex();
}

void publish(const WifiAp* aps, int n) {
  if (n > MAXN) n = MAXN;
  if (n < 0) n = 0;
  Lock lk;
  for (int i = 0; i < n; ++i) g_live[i] = aps[i];
  g_n = n;
}

int snapshot(WifiAp* out, int maxOut) {
  int n;
  {
    Lock lk;
    n = g_n < maxOut ? g_n : maxOut;
    for (int i = 0; i < n; ++i) out[i] = g_live[i];
  }
  // Sort strongest signal first (small n, insertion sort is fine).
  for (int i = 1; i < n; ++i) {
    WifiAp key = out[i];
    int j = i - 1;
    while (j >= 0 && out[j].rssi < key.rssi) {
      out[j + 1] = out[j];
      --j;
    }
    out[j + 1] = key;
  }
  return n;
}

}  // namespace wifiaps
