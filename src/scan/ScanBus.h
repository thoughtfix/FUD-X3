// ScanBus: the queue that carries classified detections from the scan task to
// the UI task. Both scanners post here; the UI loop drains it. Bounded, lossy
// under flood (never buffers unboundedly).
#pragma once
#include "ScanTypes.h"

class Detections;

namespace scanbus {
void init();
void enqueue(const ScanEvent& e);  // non-blocking; dropped if full
void drainInto(Detections& det, uint32_t now);
bool receive(ScanEvent& out);      // dequeue one; false if empty
}  // namespace scanbus
