#include "SdStore.h"
#include <Arduino.h>
#include <SDCardManager.h>
#include <string.h>

bool SdStore::mount() {
  present_ = SdMan.begin();
  return present_;
}

bool SdStore::tryRemount() {
  if (present_) return true;
  return mount();
}

bool SdStore::ensureDir(const char* path) {
  if (!present_) return false;
  return SdMan.ensureDirectoryExists(path);
}

bool SdStore::exists(const char* path) {
  if (!present_) return false;
  return SdMan.exists(path);
}

bool SdStore::remove(const char* path) {
  if (!present_) return false;
  return SdMan.remove(path);
}

uint64_t SdStore::fileSize(const char* path) {
  if (!present_) return 0;
  FsFile f = SdMan.open(path, O_RDONLY);
  if (!f) return 0;
  const uint64_t sz = f.size();
  f.close();
  return sz;
}

bool SdStore::appendLine(const char* path, const char* line) {
  if (!present_) return false;
  FsFile f = SdMan.open(path, O_WRITE | O_CREAT | O_APPEND);
  if (!f) {
    present_ = false;  // card likely pulled; keep running, retry remount later
    return false;
  }
  f.print(line);
  f.print('\n');
  f.close();
  return true;
}
