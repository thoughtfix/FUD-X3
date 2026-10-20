// Detections: the live, in-RAM store of what's been seen. Deduplicates by
// device address, ages entries out, and collapses all AirTags into a single
// "AirTag beacon" whose count is the number of distinct non-expired MACs seen
// in the last 20 minutes (per the design decisions — AirTags MAC-hop, so
// per-MAC logging is noise).
//
// Fixed-size, no heap: this board has no PSRAM, so everything is bounded.
#pragma once
#include <stdint.h>
#include "Category.h"
#include "ScanTypes.h"

// A read-only view of one detection, for the detail/list screens.
struct DetView {
  const char* label;
  char name[24];        // advertised name / SSID ("" if none)
  uint8_t key[6];
  int8_t rssi;
  char conf;
  uint32_t firstSeen;   // millis
  uint32_t lastSeen;    // millis
  uint32_t hits;        // times observed
  bool collapsed;       // true for the AirTag beacon aggregate
  int collapsedCount;   // distinct MACs behind a collapsed entry
};

class Detections {
 public:
  bool apply(const ScanEvent& e, uint32_t now);  // true if a new entry
  void tick(uint32_t now);  // age out expired entries

  int count(Cat c) const;   // distinct active entries (AirTags collapse to 1)
  int seenCount(Cat c) const;  // cumulative distinct seen since the last reset
  void resetCounts();          // clear the cumulative tally (new day / manual)
  int airtagBeacons() const;  // distinct active AirTag MACs
  uint32_t seq() const { return seq_; }  // bumps whenever counts change

  // Acknowledge: snapshot every threat currently present as "known" so Zuse
  // settles. Keyed by threat signature (category + label), not by address, so
  // a hacker tool that rotates its BLE MAC stays acked instead of re-alerting
  // on every hop. A genuinely new kind of threat (a new label) still trips it.
  void baseline(uint32_t now);
  bool hasAlert(Cat c) const;  // an un-acked threat is present in category c

  // Fill `out` with the n-th active entry in category c (slot order, with the
  // AirTag aggregate last for Trackers). Returns false if n is out of range.
  bool viewAt(Cat c, int n, DetView& out) const;

 private:
  static constexpr int MAXDET = 96;
  static constexpr int MAXAIR = 64;
  static constexpr uint32_t EVICT_MS = 120000;       // 2 min, most devices
  static constexpr uint32_t EVICT_TRK_MS = 300000;   // 5 min, fixed trackers
  static constexpr uint32_t EVICT_AIR_MS = 1200000;  // 20 min, AirTag MACs

  struct Det {
    bool used;
    uint8_t key[6];
    Cat cat;
    const char* label;
    char name[24];
    int8_t rssi;
    char conf;
    uint32_t first;
    uint32_t last;
    uint32_t hits;
  };
  struct Air {
    bool used;
    uint8_t key[6];
    uint32_t first;
    uint32_t last;
  };

  // Acknowledged threat signatures (category + label). hasAlert() stays quiet
  // for any threat whose signature is in here until the next ack rebuilds it.
  struct AckSig {
    bool used;
    Cat cat;
    const char* label;
  };
  static constexpr int MAXACK = 32;

  Det d_[MAXDET] = {};
  Air air_[MAXAIR] = {};
  AckSig ack_[MAXACK] = {};
  // Cumulative "seen this period" tally for the headline counts (home + scan
  // summaries). Lives apart from the evicting live store so the numbers don't
  // drop to zero as devices leave. AirTags use a peak-concurrent figure so one
  // MAC-hopping tag doesn't run the tracker tally up.
  uint16_t seen_[(int)Cat::COUNT] = {};
  uint16_t airtagPeak_ = 0;
  uint32_t seq_ = 0;

  bool applyAirtag(const uint8_t key[6], uint32_t now);  // true if a new MAC
  bool isAcked(Cat c, const char* label) const;
  void ackCurrent();  // replace ack_ with a snapshot of what's present now
};
