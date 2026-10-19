#include "ScanCoordinator.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "BleScanner.h"
#include "ScanBus.h"
#include "WifiScanner.h"

static BleScanner s_ble;
static WifiScanner s_wifi;
static volatile char s_active = '-';

// Rotation: BLE for ~6s (WiFi off), then a WiFi window (AP scan + deauth
// monitor, BLE idle), then back. Mirrors FUD's scanTask duty cycle.
static void scanTask(void*) {
  for (;;) {
    // BLE window: bring the BT controller up, scan, tear it down so its memory
    // is free before WiFi needs it.
    s_active = 'B';
    s_ble.start();
    s_ble.scanWindow(6000);
    s_ble.stop();

    // WiFi window: now there's room to init WiFi. Scan + deauth, then release.
    s_active = 'W';
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    s_wifi.scanAps();
    s_wifi.monitorDeauth(900);  // 6 hops x 900ms ~= 5.4s
    WiFi.mode(WIFI_OFF);        // frees the WiFi stack for the next BLE window
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void ScanCoordinator::begin() {
  scanbus::init();
  xTaskCreate(scanTask, "scan", 8192, nullptr, 1, nullptr);
}

char ScanCoordinator::activeRadio() const { return s_active; }
