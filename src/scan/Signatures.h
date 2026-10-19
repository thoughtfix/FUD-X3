// FUD-X3 BLE detection signatures.
//
// Ported from FUD's own signatures.h (thoughtfix, GPLv3). These are factual
// radio identifiers (Bluetooth SIG company IDs, GATT service UUIDs), not
// copyrightable expression; the subset and confidence grades trace to FUD and,
// through it, the SquachWatch-CYD catalog (GPL-3.0) and the OUI-SPY project.
//
// Only BLE signatures live here for now; Wi-Fi SSID/OUI signatures arrive with
// the Wi-Fi scanner. Include this from one translation unit (BleScanner.cpp).
#pragma once
#include <stdint.h>
#include "Category.h"

// service 0xFFFF = don't care; company 0xFFFF = don't care.
struct BleSig {
  uint16_t service;
  uint16_t company;
  Cat cat;
  const char* label;
  char conf;
};

static const BleSig BLE_SIGS[] = {
    {0xFEED, 0xFFFF, Cat::Tracker, "Tile", 'H'},
    {0xFEEC, 0xFFFF, Cat::Tracker, "Tile", 'H'},
    {0xFD5A, 0xFFFF, Cat::Tracker, "SmartTag", 'H'},   // Samsung SmartTag
    {0xFEAA, 0xFFFF, Cat::Tracker, "GoogleFMD", 'M'},  // Google Find My (Eddystone-shared)
    {0xFFFF, 0x09C8, Cat::Camera, "Flock", 'H'},       // Flock/XUNTONG mfg company ID
    {0xFFFF, 0x034D, Cat::Camera, "Axon", 'H'},        // TASER Intl (Axon body cams)
    {0xFFFA, 0xFFFF, Cat::Drone, "RemoteID", 'M'},     // ASTM F3411 drone Remote ID
};
static const int BLE_SIGS_N = sizeof(BLE_SIGS) / sizeof(BLE_SIGS[0]);

// Composite / payload checks (handled in classify code, not the table):
// Meta/Ray-Ban glasses: Luxottica CID 0x0D53 AND Meta svc UUID 0xFD5F together
//   (single 0xFD5F false-positives on phones running Meta apps).
#define META_CID 0x0D53
#define META_SVC 0xFD5F
// Apple Find My / AirTag: company 0x004C with offline-finding type byte 0x12.
#define APPLE_CID 0x004C
#define FINDMY_TYPE 0x12

// Stable label used to collapse all AirTag/Find My adverts into one beacon.
#define AIRTAG_LABEL "AirTag"

// ---- Wi-Fi: SSID-prefix signatures ----
struct SsidSig {
  const char* prefix;
  Cat cat;
  const char* label;
  char conf;
};
static const SsidSig SSID_SIGS[] = {
    {"AB2-", Cat::Camera, "Axon", 'M'},   // Axon body cam SSIDs
    {"AB3-", Cat::Camera, "Axon", 'M'},
    {"AB4-", Cat::Camera, "Axon", 'M'},
    {"AXON-", Cat::Camera, "Axon", 'M'},
    {"Pineapple_", Cat::HackDev, "PineAP", 'M'},  // WiFi Pineapple
};
static const int SSID_SIGS_N = sizeof(SSID_SIGS) / sizeof(SSID_SIGS[0]);

// ---- Wi-Fi OUI signatures (first 3 MAC bytes) ----
// A match is a candidate, not proof. Ported from FUD; the surveillance-vendor
// block derives from SquachWatch-CYD's kOuiTable (GPL-3.0), reformatted here.
struct OuiSig {
  uint8_t oui[3];
  Cat cat;
  const char* label;
  char conf;
};
static const OuiSig OUI_SIGS[] = {
    {{0x18, 0xB4, 0x30}, Cat::Camera, "Nest", 'H'},
    {{0x64, 0x16, 0x66}, Cat::Camera, "Nest", 'H'},
    {{0x48, 0x62, 0x64}, Cat::Camera, "Arlo", 'H'},
    {{0xA4, 0x11, 0x62}, Cat::Camera, "Arlo", 'H'},
    {{0xFC, 0x9C, 0x98}, Cat::Camera, "Arlo", 'H'},
    {{0x98, 0xED, 0x5C}, Cat::Camera, "Tesla", 'M'},
    {{0x48, 0x05, 0x60}, Cat::Camera, "Meta", 'M'},
    {{0x50, 0x99, 0x03}, Cat::Camera, "Meta", 'M'},
    {{0x78, 0xC4, 0xFA}, Cat::Camera, "Meta", 'M'},
    {{0x80, 0xF3, 0xEF}, Cat::Camera, "Meta", 'M'},
    {{0x84, 0x57, 0xF7}, Cat::Camera, "Meta", 'M'},
    {{0x88, 0x25, 0x08}, Cat::Camera, "Meta", 'M'},
    {{0x94, 0xF9, 0x29}, Cat::Camera, "Meta", 'M'},
    {{0xB4, 0x17, 0xA8}, Cat::Camera, "Meta", 'M'},
    {{0xC0, 0xDD, 0x8A}, Cat::Camera, "Meta", 'M'},
    {{0xCC, 0xA1, 0x74}, Cat::Camera, "Meta", 'M'},
    {{0xD0, 0xB3, 0xC2}, Cat::Camera, "Meta", 'M'},
    {{0xD4, 0xD6, 0x59}, Cat::Camera, "Meta", 'M'},
    {{0xF4, 0x4E, 0x35}, Cat::Camera, "Meta", 'M'},
    {{0x04, 0xA8, 0x5A}, Cat::Drone, "DJI", 'H'},
    {{0x0C, 0x9A, 0xE6}, Cat::Drone, "DJI", 'H'},
    {{0x34, 0xD2, 0x62}, Cat::Drone, "DJI", 'H'},
    {{0x48, 0x1C, 0xB9}, Cat::Drone, "DJI", 'H'},
    {{0x4C, 0x43, 0xF6}, Cat::Drone, "DJI", 'H'},
    {{0x58, 0xB8, 0x58}, Cat::Drone, "DJI", 'H'},
    {{0x60, 0x60, 0x1F}, Cat::Drone, "DJI", 'H'},
    {{0x88, 0x29, 0x85}, Cat::Drone, "DJI", 'H'},
    {{0x8C, 0x58, 0x23}, Cat::Drone, "DJI", 'H'},
    {{0xE4, 0x7A, 0x2C}, Cat::Drone, "DJI", 'H'},
    {{0x00, 0x25, 0xDF}, Cat::Camera, "Axon", 'H'},
    {{0xB4, 0x1E, 0x52}, Cat::Camera, "Flock", 'H'},
    {{0x00, 0x04, 0x7D}, Cat::Camera, "ALPR", 'H'},
    {{0x00, 0x18, 0x85}, Cat::Camera, "ALPR", 'H'},
    {{0x00, 0x1F, 0x92}, Cat::Camera, "ALPR", 'H'},
    {{0x4C, 0xCC, 0x34}, Cat::Camera, "ALPR", 'H'},
    {{0xB8, 0xE2, 0x8C}, Cat::Camera, "ALPR", 'H'},
    {{0x00, 0xBF, 0x15}, Cat::Camera, "ALPR", 'H'},
    {{0x0C, 0xBF, 0x15}, Cat::Camera, "ALPR", 'H'},
    {{0x2C, 0xAA, 0x8E}, Cat::Camera, "Wyze", 'H'},
    {{0xD0, 0x3F, 0x27}, Cat::Camera, "Wyze", 'H'},
    {{0x7C, 0x78, 0xB2}, Cat::Camera, "Wyze", 'H'},
    {{0x34, 0xD2, 0x70}, Cat::Camera, "Amazon", 'M'},
    {{0xF0, 0x27, 0x2D}, Cat::Camera, "Amazon", 'M'},
    {{0xC0, 0x56, 0xE3}, Cat::Camera, "Hikvision", 'H'},
    {{0x44, 0x19, 0xB6}, Cat::Camera, "Hikvision", 'H'},
    {{0x28, 0x57, 0xBE}, Cat::Camera, "Hikvision", 'H'},
    {{0xE0, 0xA7, 0x00}, Cat::Camera, "Verkada", 'H'},
    {{0x70, 0x1A, 0xD5}, Cat::Camera, "Avigilon", 'H'},
    {{0x00, 0x40, 0x8C}, Cat::Camera, "Axis", 'H'},
    {{0xB8, 0xA4, 0x4F}, Cat::Camera, "Axis", 'H'},
    {{0xFC, 0x65, 0xDE}, Cat::Camera, "Ring", 'M'},
    {{0x68, 0x37, 0xE9}, Cat::Camera, "Ring", 'M'},
    {{0xAC, 0x9F, 0xC3}, Cat::Camera, "Ring", 'H'},
    {{0x18, 0x7F, 0x88}, Cat::Camera, "Ring", 'H'},
    {{0x34, 0x3E, 0xA4}, Cat::Camera, "Ring", 'H'},
    {{0x54, 0xE0, 0x19}, Cat::Camera, "Ring", 'H'},
    {{0x5C, 0x47, 0x5E}, Cat::Camera, "Ring", 'H'},
    {{0x64, 0x9A, 0x63}, Cat::Camera, "Ring", 'H'},
    {{0x90, 0x48, 0x6C}, Cat::Camera, "Ring", 'H'},
    {{0x9C, 0x76, 0x13}, Cat::Camera, "Ring", 'H'},
    {{0xCC, 0x3B, 0xFB}, Cat::Camera, "Ring", 'H'},
    {{0xC4, 0xDB, 0xAD}, Cat::Camera, "Ring", 'H'},
    {{0x24, 0x2B, 0xD6}, Cat::Camera, "Ring", 'H'},
    {{0x00, 0xB4, 0x63}, Cat::Camera, "Ring", 'H'},
    {{0x50, 0xE4, 0x67}, Cat::Camera, "Ring", 'H'},
};
static const int OUI_SIGS_N = sizeof(OUI_SIGS) / sizeof(OUI_SIGS[0]);

// Stable collapse keys (one entry regardless of spoofed/hopping source MAC).
#define PWNED_SSID "pwned"

// ---- BLE pairing-popup spam (flood of these from rotating MACs) ----
#define MS_CID 0x0006           // Microsoft SwiftPair
#define FASTPAIR_SVC 0xFE2C     // Google Fast Pair
#define APPLE_PROX_PAIR 0x07    // Apple proximity pairing (AirPods popup)
#define APPLE_NEARBY_ACT 0x0F   // Apple nearby action (setup popup)
#define BLE_SPAM_THRESH 18      // spammy adverts in one BLE window = attack

// ---- BLE card-skimmer serial modules, matched by advertised name ----
static const char* const SKIMMER_NAMES[] = {"HC-05", "HC-06", "HM-10",
                                            "HM-11", "JDY-",  "BT04",
                                            "MLT-BT05"};
static const int SKIMMER_NAMES_N =
    sizeof(SKIMMER_NAMES) / sizeof(SKIMMER_NAMES[0]);

// ---- Pwnagotchi: promiscuous beacon from a DE:AD:BE:EF source MAC ----
static const uint8_t PWNAGOTCHI_OUI[4] = {0xDE, 0xAD, 0xBE, 0xEF};
