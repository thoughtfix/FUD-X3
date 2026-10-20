#include "Detections.h"
#include <string.h>
#include "Signatures.h"  // for AIRTAG_LABEL

static bool keyEq(const uint8_t a[6], const uint8_t b[6]) {
  return memcmp(a, b, 6) == 0;
}

// Trackers that rotate their BLE address for privacy. Each is collapsed per
// type into a single "beacon" count instead of inflating the device tally.
static const char* const kHopperLabels[] = {AIRTAG_LABEL, "Tile", "SmartTag",
                                            "GoogleFMD"};
static bool isHopper(const char* l) {
  if (!l) return false;
  for (const char* h : kHopperLabels)
    if (strcmp(l, h) == 0) return true;
  return false;
}
static bool isAirtag(const char* l) {
  return l && strcmp(l, AIRTAG_LABEL) == 0;
}
// The display label for a collapsed group, e.g. "AirTag" -> "AirTag beacons".
static const char* beaconsLabel(const char* t) {
  if (!t) return "beacons";
  if (strcmp(t, AIRTAG_LABEL) == 0) return AIRTAG_LABEL " beacons";
  if (strcmp(t, "Tile") == 0) return "Tile beacons";
  if (strcmp(t, "SmartTag") == 0) return "SmartTag beacons";
  if (strcmp(t, "GoogleFMD") == 0) return "GoogleFMD beacons";
  return t;
}

bool Detections::apply(const ScanEvent& e, uint32_t now) {
  if (e.cat == Cat::Tracker && isHopper(e.label)) {
    return applyHopper(e.key, e.label, now);
  }

  // Update an existing entry with this address, or claim a free slot.
  int free = -1;
  for (int i = 0; i < MAXDET; ++i) {
    if (!d_[i].used) {
      if (free < 0) free = i;
      continue;
    }
    if (keyEq(d_[i].key, e.key)) {
      d_[i].last = now;
      d_[i].rssi = e.rssi;
      ++d_[i].hits;
      return false;  // already counted; just refresh
    }
  }
  if (free < 0) return false;  // table full: drop (bounded, no heap)

  Det& d = d_[free];
  d.used = true;
  memcpy(d.key, e.key, 6);
  d.cat = e.cat;
  d.label = e.label;
  memcpy(d.name, e.name, sizeof(d.name));
  d.rssi = e.rssi;
  d.conf = e.conf;
  d.first = d.last = now;
  d.hits = 1;
  if ((int)d.cat < (int)Cat::COUNT) ++seen_[(int)d.cat];  // cumulative tally
  ++seq_;
  return true;
}

bool Detections::applyHopper(const uint8_t key[6], const char* label,
                             uint32_t now) {
  int free = -1;
  for (int i = 0; i < MAXAIR; ++i) {
    if (!air_[i].used) {
      if (free < 0) free = i;
      continue;
    }
    if (keyEq(air_[i].key, key)) {
      air_[i].last = now;
      air_[i].label = label;
      return false;
    }
  }
  if (free < 0) return false;
  air_[free].used = true;
  memcpy(air_[free].key, key, 6);
  air_[free].label = label;
  air_[free].first = air_[free].last = now;
  updateHopperPeak();
  ++seq_;  // a new distinct beacon changes the displayed count
  return true;
}

void Detections::updateHopperPeak() {
  int total = 0, air = 0;
  for (int i = 0; i < MAXAIR; ++i) {
    if (!air_[i].used) continue;
    ++total;
    if (isAirtag(air_[i].label)) ++air;
  }
  if (total > hopperPeak_) {
    hopperPeak_ = (uint16_t)total;
    airtagAtPeak_ = (uint16_t)air;  // airtag share recorded at the peak moment
  }
}

int Detections::beaconCount(const char* label) const {
  int n = 0;
  for (int i = 0; i < MAXAIR; ++i)
    if (air_[i].used && air_[i].label && label &&
        strcmp(air_[i].label, label) == 0)
      ++n;
  return n;
}

int Detections::presentHoppers(const char* labels[], int counts[],
                               uint32_t newest[], int max) const {
  int nl = 0;
  for (int i = 0; i < MAXAIR; ++i) {
    if (!air_[i].used) continue;
    const char* L = air_[i].label;
    int f = -1;
    for (int k = 0; k < nl; ++k)
      if (labels[k] && L && strcmp(labels[k], L) == 0) {
        f = k;
        break;
      }
    if (f < 0) {
      if (nl < max) {
        labels[nl] = L;
        counts[nl] = 1;
        newest[nl] = air_[i].last;
        ++nl;
      }
    } else {
      ++counts[f];
      if (air_[i].last > newest[f]) newest[f] = air_[i].last;
    }
  }
  return nl;
}

bool Detections::isAcked(Cat c, const char* label) const {
  if (!label) return false;
  for (int i = 0; i < MAXACK; ++i)
    if (ack_[i].used && ack_[i].cat == c && ack_[i].label &&
        strcmp(ack_[i].label, label) == 0)
      return true;
  return false;
}

void Detections::ackCurrent() {
  for (int i = 0; i < MAXACK; ++i) ack_[i].used = false;  // rebuild from scratch
  auto add = [&](Cat c, const char* lab) {
    if (!lab) return;
    // Only the alarming categories can raise the face, so only those are worth
    // acking. Open Wi-Fi (and anything non-alerting) never stirs Zuse.
    if (c != Cat::Attack && c != Cat::HackDev && c != Cat::Camera &&
        c != Cat::Drone && c != Cat::Tracker)
      return;
    if (isAcked(c, lab)) return;  // dedup
    for (int i = 0; i < MAXACK; ++i)
      if (!ack_[i].used) {
        ack_[i] = {true, c, lab};
        return;
      }
  };
  for (int i = 0; i < MAXDET; ++i)
    if (d_[i].used) add(d_[i].cat, d_[i].label);
  const char* labels[8];
  int counts[8];
  uint32_t newest[8];
  const int nl = presentHoppers(labels, counts, newest, 8);
  for (int g = 0; g < nl; ++g) add(Cat::Tracker, labels[g]);
}

void Detections::baseline(uint32_t /*now*/) {
  ackCurrent();
  ++seq_;
}

bool Detections::hasAlert(Cat c) const {
  for (int i = 0; i < MAXDET; ++i)
    if (d_[i].used && d_[i].cat == c && !isAcked(c, d_[i].label)) return true;
  if (c == Cat::Tracker) {
    const char* labels[8];
    int counts[8];
    uint32_t newest[8];
    const int nl = presentHoppers(labels, counts, newest, 8);
    for (int g = 0; g < nl; ++g)
      if (!isAcked(Cat::Tracker, labels[g])) return true;
  }
  return false;
}

void Detections::tick(uint32_t now) {
  for (int i = 0; i < MAXDET; ++i) {
    if (!d_[i].used) continue;
    const uint32_t lim = (d_[i].cat == Cat::Tracker) ? EVICT_TRK_MS : EVICT_MS;
    if (now - d_[i].last > lim) {
      d_[i].used = false;
      ++seq_;
    }
  }
  for (int i = 0; i < MAXAIR; ++i) {
    if (air_[i].used && now - air_[i].last > EVICT_AIR_MS) {
      air_[i].used = false;
      ++seq_;
    }
  }
}

int Detections::airtagBeacons() const { return beaconCount(AIRTAG_LABEL); }

bool Detections::viewAt(Cat c, int n, DetView& out) const {
  int seen = 0;
  for (int i = 0; i < MAXDET; ++i) {
    if (!d_[i].used || d_[i].cat != c) continue;
    if (seen == n) {
      memcpy(out.key, d_[i].key, 6);
      out.label = d_[i].label;
      memcpy(out.name, d_[i].name, sizeof(out.name));
      out.rssi = d_[i].rssi;
      out.conf = d_[i].conf;
      out.firstSeen = d_[i].first;
      out.lastSeen = d_[i].last;
      out.hits = d_[i].hits;
      out.collapsed = false;
      out.collapsedCount = 0;
      return true;
    }
    ++seen;
  }
  // After the fixed trackers, one collapsed row per hopper type present
  // (AirTag beacons, Tile beacons, ...).
  if (c == Cat::Tracker) {
    const char* labels[8];
    int counts[8];
    uint32_t newest[8];
    const int nl = presentHoppers(labels, counts, newest, 8);
    for (int g = 0; g < nl; ++g) {
      if (seen == n) {
        memset(out.key, 0, 6);
        out.label = beaconsLabel(labels[g]);
        out.name[0] = 0;
        out.rssi = 0;
        out.conf = 'H';
        out.firstSeen = newest[g];
        out.lastSeen = newest[g];
        out.hits = counts[g];
        out.collapsed = true;
        out.collapsedCount = counts[g];
        return true;
      }
      ++seen;
    }
  }
  return false;
}

int Detections::count(Cat c) const {
  int n = 0;
  for (int i = 0; i < MAXDET; ++i)
    if (d_[i].used && d_[i].cat == c) ++n;
  // Each hopper type present (AirTag, Tile, ...) is one collapsed row.
  if (c == Cat::Tracker) {
    const char* labels[8];
    int counts[8];
    uint32_t newest[8];
    n += presentHoppers(labels, counts, newest, 8);
  }
  return n;
}

int Detections::seenCount(Cat c) const {
  int n = seen_[(int)c];
  if (c == Cat::Tracker) n += hopperPeak_;  // peak hopper beacons, not every MAC
  return n;
}

void Detections::resetCounts() {
  for (int i = 0; i < (int)Cat::COUNT; ++i) seen_[i] = 0;
  hopperPeak_ = 0;
  airtagAtPeak_ = 0;
  ++seq_;
}
