#include "Detections.h"
#include <string.h>
#include "Signatures.h"  // for AIRTAG_LABEL

static bool keyEq(const uint8_t a[6], const uint8_t b[6]) {
  return memcmp(a, b, 6) == 0;
}

bool Detections::apply(const ScanEvent& e, uint32_t now) {
  if (e.label && strcmp(e.label, AIRTAG_LABEL) == 0) {
    return applyAirtag(e.key, now);
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
  ++seq_;
  return true;
}

bool Detections::applyAirtag(const uint8_t key[6], uint32_t now) {
  int free = -1;
  for (int i = 0; i < MAXAIR; ++i) {
    if (!air_[i].used) {
      if (free < 0) free = i;
      continue;
    }
    if (keyEq(air_[i].key, key)) {
      air_[i].last = now;
      return false;
    }
  }
  if (free < 0) return false;
  air_[free].used = true;
  memcpy(air_[free].key, key, 6);
  air_[free].first = air_[free].last = now;
  ++seq_;  // a new distinct beacon changes the displayed count
  return true;
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
    if (isAcked(c, lab)) return;  // dedup
    for (int i = 0; i < MAXACK; ++i)
      if (!ack_[i].used) {
        ack_[i] = {true, c, lab};
        return;
      }
  };
  for (int i = 0; i < MAXDET; ++i)
    if (d_[i].used) add(d_[i].cat, d_[i].label);
  if (airtagBeacons() > 0) add(Cat::Tracker, AIRTAG_LABEL);
}

void Detections::baseline(uint32_t /*now*/) {
  ackCurrent();
  ++seq_;
}

bool Detections::hasAlert(Cat c) const {
  for (int i = 0; i < MAXDET; ++i)
    if (d_[i].used && d_[i].cat == c && !isAcked(c, d_[i].label)) return true;
  if (c == Cat::Tracker && airtagBeacons() > 0 &&
      !isAcked(Cat::Tracker, AIRTAG_LABEL))
    return true;
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

int Detections::airtagBeacons() const {
  int n = 0;
  for (int i = 0; i < MAXAIR; ++i)
    if (air_[i].used) ++n;
  return n;
}

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
  // The collapsed AirTag beacon is the last tracker entry.
  if (c == Cat::Tracker && airtagBeacons() > 0 && seen == n) {
    int count = 0;
    uint32_t newest = 0;
    for (int i = 0; i < MAXAIR; ++i) {
      if (!air_[i].used) continue;
      ++count;
      if (air_[i].last > newest) newest = air_[i].last;
    }
    memset(out.key, 0, 6);
    out.label = AIRTAG_LABEL " beacons";
    out.name[0] = 0;
    out.rssi = 0;
    out.conf = 'H';
    out.firstSeen = newest;
    out.lastSeen = newest;
    out.hits = count;
    out.collapsed = true;
    out.collapsedCount = count;
    return true;
  }
  return false;
}

int Detections::count(Cat c) const {
  int n = 0;
  for (int i = 0; i < MAXDET; ++i)
    if (d_[i].used && d_[i].cat == c) ++n;
  // The collapsed AirTag beacon is one logical tracker entry.
  if (c == Cat::Tracker && airtagBeacons() > 0) ++n;
  return n;
}
