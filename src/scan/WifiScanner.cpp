#include "WifiScanner.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include "IgnoreList.h"
#include "ScanBus.h"
#include "Signatures.h"
#include "WifiApList.h"

// Ported from FUD's scanWifiOnce / wifiMonitorDeauth (thoughtfix, GPLv3).

static void enqueue(const uint8_t key[6], Cat cat, const char* label,
                    const char* name, int8_t rssi, char conf) {
  ScanEvent e;
  memcpy(e.key, key, 6);
  e.cat = cat;
  e.label = label;
  strncpy(e.name, name ? name : "", sizeof(e.name) - 1);
  e.name[sizeof(e.name) - 1] = 0;
  e.rssi = rssi;
  e.conf = conf;
  scanbus::enqueue(e);
}

// A stable 6-byte key from an SSID (FNV-1a), so all BSSIDs of one network
// collapse to a single entry.
static void ssidKey(const char* s, uint8_t k[6]) {
  uint32_t h = 2166136261u;
  for (const char* p = s; *p; ++p) {
    h ^= (uint8_t)*p;
    h *= 16777619u;
  }
  k[0] = 0xE7;
  k[1] = h;
  k[2] = h >> 8;
  k[3] = h >> 16;
  k[4] = h >> 24;
  k[5] = (uint8_t)strlen(s);
}

void WifiScanner::scanAps() {
  const int n = WiFi.scanNetworks(false, true);

  // Snapshot every network for the informational "Wi-Fi Networks" view,
  // deduplicated by SSID (keep the strongest). This is display-only and spans
  // all networks, including classified and ignored ones.
  WifiAp staging[wifiaps::MAXN];
  int sn = 0;

  for (int i = 0; i < n; ++i) {
    String ss = WiFi.SSID(i);
    if (ss.length() == 0) continue;

    const bool apOpen = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    const int8_t apRssi = WiFi.RSSI(i);
    int found = -1;
    for (int k = 0; k < sn; ++k)
      if (!strcmp(staging[k].ssid, ss.c_str())) { found = k; break; }
    if (found >= 0) {
      if (apRssi > staging[found].rssi) {
        staging[found].rssi = apRssi;
        staging[found].open = apOpen;
      }
    } else if (sn < wifiaps::MAXN) {
      strncpy(staging[sn].ssid, ss.c_str(), sizeof(staging[sn].ssid) - 1);
      staging[sn].ssid[sizeof(staging[sn].ssid) - 1] = 0;
      staging[sn].rssi = apRssi;
      staging[sn].open = apOpen;
      ++sn;
    }

    if (ignores.contains(ss.c_str())) continue;  // user's own network
    uint8_t* b = WiFi.BSSID(i);
    if (!b) continue;
    uint8_t key[6];
    memcpy(key, b, 6);
    const int8_t rssi = WiFi.RSSI(i);

    // Evil twin: same SSID advertised both open and secured (a clone running
    // open to sniff). Encryption-only differences (WPA2 vs WPA3) are normal.
    const bool iOpen = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    bool twin = false;
    for (int j = 0; j < n; ++j) {
      if (j != i && WiFi.SSID(j) == ss &&
          (WiFi.encryptionType(j) == WIFI_AUTH_OPEN) != iOpen) {
        twin = true;
        break;
      }
    }
    if (twin) {
      uint8_t tk[6];
      ssidKey(ss.c_str(), tk);
      enqueue(tk, Cat::Attack, "evil twin", ss.c_str(), rssi, 'M');
      continue;
    }
    if (ss == PWNED_SSID) {
      enqueue(key, Cat::HackDev, "pwned", ss.c_str(), rssi, 'H');
      continue;
    }
    bool matched = false;
    for (int k = 0; k < SSID_SIGS_N; ++k) {
      if (ss.startsWith(SSID_SIGS[k].prefix)) {
        enqueue(key, SSID_SIGS[k].cat, SSID_SIGS[k].label, ss.c_str(),
                rssi, SSID_SIGS[k].conf);
        matched = true;
        break;
      }
    }
    bool ouiMatched = false;
    if (!matched) {
      for (int k = 0; k < OUI_SIGS_N; ++k) {
        if (!memcmp(key, OUI_SIGS[k].oui, 3)) {
          enqueue(key, OUI_SIGS[k].cat, OUI_SIGS[k].label, ss.c_str(),
                  rssi, OUI_SIGS[k].conf);
          ouiMatched = true;
          break;
        }
      }
    }
    // Open network with no threat signature: informational, logged only.
    if (!matched && !ouiMatched && iOpen)
      enqueue(key, Cat::OpenWifi, "Open", ss.c_str(), rssi, 'L');
  }
  wifiaps::publish(staging, sn);
  WiFi.scanDelete();
}

// ---- Promiscuous deauth/disassoc detector ----
static const uint32_t DEAUTH_THRESH = 10;  // deauth frames in a window = attack
static volatile uint32_t g_deauth = 0, g_disassoc = 0;
static volatile uint8_t g_src[6] = {0};
static volatile uint8_t g_bcast = 0;
static volatile uint16_t g_reason = 0;
static volatile bool g_pwn = false;  // Pwnagotchi beacon seen this window

static void promiscCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const uint8_t* f = ((wifi_promiscuous_pkt_t*)buf)->payload;
  const uint8_t fc = f[0];
  const uint8_t ftype = (fc & 0x0C) >> 2, sub = (fc & 0xF0) >> 4;
  if (ftype != 0) return;  // management frames only
  if (sub == 8) {          // beacon: Pwnagotchi broadcasts from DE:AD:BE:EF
    if (f[10] == PWNAGOTCHI_OUI[0] && f[11] == PWNAGOTCHI_OUI[1] &&
        f[12] == PWNAGOTCHI_OUI[2] && f[13] == PWNAGOTCHI_OUI[3])
      g_pwn = true;
    return;
  }
  if (sub == 10) {
    g_disassoc++;
    return;
  }
  if (sub == 12) {  // deauth
    g_deauth++;
    for (int i = 0; i < 6; ++i) g_src[i] = f[10 + i];  // addr2 = transmitter
    bool bc = true;
    for (int i = 0; i < 6; ++i)
      if (f[4 + i] != 0xFF) bc = false;  // addr1 = dest
    if (bc) g_bcast = 1;
    g_reason = (uint16_t)f[24] | ((uint16_t)f[25] << 8);
  }
}

void WifiScanner::monitorDeauth(uint32_t perChanMs) {
  g_deauth = 0;
  g_disassoc = 0;
  g_bcast = 0;
  g_reason = 0;
  g_pwn = false;
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&promiscCb);
  static const uint8_t chans[] = {1, 6, 11, 1, 6, 11};
  for (uint8_t c : chans) {
    esp_wifi_set_channel(c, WIFI_SECOND_CHAN_NONE);
    vTaskDelay(pdMS_TO_TICKS(perChanMs));
  }
  esp_wifi_set_promiscuous(false);

  if (g_deauth >= DEAUTH_THRESH) {
    // Sources are usually spoofed, so collapse the whole flood into one entry.
    static const uint8_t dkey[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
    enqueue(dkey, Cat::Attack, "deauth", "", -50, 'H');
  }
  if (g_pwn) {
    static const uint8_t pkey[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x01};
    enqueue(pkey, Cat::HackDev, "Pwnagotchi", "", -50, 'H');
  }
}
