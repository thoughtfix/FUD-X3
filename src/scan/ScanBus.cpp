#include "ScanBus.h"
#include <Arduino.h>
#include "Detections.h"

static QueueHandle_t q = nullptr;

void scanbus::init() {
  if (!q) q = xQueueCreate(48, sizeof(ScanEvent));
}

void scanbus::enqueue(const ScanEvent& e) {
  if (q) xQueueSend(q, &e, 0);
}

void scanbus::drainInto(Detections& det, uint32_t now) {
  if (!q) return;
  ScanEvent e;
  while (xQueueReceive(q, &e, 0) == pdTRUE) det.apply(e, now);
}

bool scanbus::receive(ScanEvent& out) {
  if (!q) return false;
  return xQueueReceive(q, &out, 0) == pdTRUE;
}
