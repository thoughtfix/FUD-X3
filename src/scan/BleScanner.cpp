#include "BleScanner.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "IgnoreList.h"
#include "ScanBus.h"
#include "Signatures.h"

// Classification mirrors FUD's classifyBle (thoughtfix, GPLv3): Flipper by name,
// the Meta-glasses CID+service composite, Apple Find My / AirTag by the
// offline-finding type byte, then the single-signature table.

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

static uint32_t g_spam = 0;  // pairing-popup adverts seen in the current window

// True when an advert looks like a pairing-popup (what BLE spam floods use).
static bool looksLikeSpam(const NimBLEAdvertisedDevice* dev, uint16_t company,
                          const std::string& md) {
  if (company == APPLE_CID && md.size() >= 3 &&
      ((uint8_t)md[2] == APPLE_PROX_PAIR || (uint8_t)md[2] == APPLE_NEARBY_ACT))
    return true;
  if (company == MS_CID) return true;  // SwiftPair
  if (dev->isAdvertisingService(NimBLEUUID((uint16_t)FASTPAIR_SVC))) return true;
  return false;
}

static void classify(const NimBLEAdvertisedDevice* dev) {
  uint8_t key[6];
  memcpy(key, dev->getAddress().getBase()->val, 6);
  const int8_t rssi = dev->getRSSI();
  const std::string nm = dev->getName();
  const char* name = nm.c_str();
  const std::string md = dev->getManufacturerData();
  const uint16_t company =
      md.size() >= 2 ? ((uint8_t)md[0] | ((uint8_t)md[1] << 8)) : 0xFFFF;

  if (looksLikeSpam(dev, company, md)) ++g_spam;  // flood tallied in scanWindow

  if (ignores.contains(name)) return;  // user's own gear

  if (nm.find("Flipper") != std::string::npos) {
    enqueue(key, Cat::HackDev, "Flipper", name, rssi, 'H');
    return;
  }
  for (int i = 0; i < SKIMMER_NAMES_N; ++i) {
    if (nm.find(SKIMMER_NAMES[i]) != std::string::npos) {
      enqueue(key, Cat::HackDev, "Skimmer", name, rssi, 'M');
      return;
    }
  }
  if (company == META_CID &&
      dev->isAdvertisingService(NimBLEUUID((uint16_t)META_SVC))) {
    enqueue(key, Cat::Camera, "Meta glass", name, rssi, 'H');
    return;
  }
  if (company == APPLE_CID && md.size() >= 3 && (uint8_t)md[2] == FINDMY_TYPE) {
    enqueue(key, Cat::Tracker, AIRTAG_LABEL, name, rssi, 'H');
    return;
  }
  for (int i = 0; i < BLE_SIGS_N; ++i) {
    const BleSig& s = BLE_SIGS[i];
    const bool svcOk = (s.service == 0xFFFF) ||
                       dev->isAdvertisingService(NimBLEUUID(s.service));
    const bool cidOk = (s.company == 0xFFFF) || (company == s.company);
    if (svcOk && cidOk) {
      enqueue(key, s.cat, s.label, name, rssi, s.conf);
      return;
    }
  }
}

void BleScanner::start() {
  NimBLEDevice::init("");
  NimBLEScan* s = NimBLEDevice::getScan();
  s->setActiveScan(true);
  s->setInterval(160);
  s->setWindow(80);
}

void BleScanner::stop() {
  // Free the BT controller memory so WiFi has room to init (no PSRAM: only one
  // radio stack is resident at a time).
  NimBLEDevice::deinit(true);
}

void BleScanner::scanWindow(uint32_t ms) {
  NimBLEScan* s = NimBLEDevice::getScan();
  g_spam = 0;
  NimBLEScanResults r = s->getResults(ms, false);
  const int n = r.getCount();
  for (int i = 0; i < n; ++i) classify(r.getDevice(i));
  s->clearResults();
  if (g_spam >= BLE_SPAM_THRESH) {
    // Collapse the whole flood into one entry (sources rotate constantly).
    static const uint8_t skey[6] = {0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};
    enqueue(skey, Cat::Attack, "BLE spam", "", -50, 'H');
  }
}
