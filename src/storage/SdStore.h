// SdStore: a thin, hot-swap-safe wrapper over the SDK's SDCardManager.
//
// Safe with no card and across removal: present() reflects the last known
// state, tryMount() re-detects an inserted card, and a failed append marks the
// card absent so the firmware keeps running (detection continues; logging just
// pauses) until a card is back. Files are opened and closed per append so a
// pulled card never leaves a dangling handle.
#pragma once
#include <stddef.h>
#include <stdint.h>

class SdStore {
 public:
  bool mount();        // try to mount; updates present()
  bool tryRemount();   // re-detect after absence (call periodically)
  bool present() const { return present_; }

  bool ensureDir(const char* path);
  bool exists(const char* path);
  bool appendLine(const char* path, const char* line);
  bool remove(const char* path);
  uint64_t fileSize(const char* path);  // 0 if missing

 private:
  bool present_ = false;
};
