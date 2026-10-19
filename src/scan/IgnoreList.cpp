#include "IgnoreList.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

IgnoreList ignores;

static SemaphoreHandle_t mtx = nullptr;

struct Lock {
  Lock() { if (mtx) xSemaphoreTake(mtx, portMAX_DELAY); }
  ~Lock() { if (mtx) xSemaphoreGive(mtx); }
};

void IgnoreList::begin() {
  if (!mtx) mtx = xSemaphoreCreateMutex();
  Lock lk;
  Preferences p;
  p.begin("fudig", true);
  n_ = p.getInt("n", 0);
  if (n_ > MAXN) n_ = MAXN;
  char key[8];
  for (int i = 0; i < n_; ++i) {
    snprintf(key, sizeof(key), "%d", i);
    String s = p.getString(key, "");
    strncpy(names_[i], s.c_str(), NAMELEN - 1);
    names_[i][NAMELEN - 1] = 0;
  }
  p.end();
}

void IgnoreList::save() {
  Preferences p;
  p.begin("fudig", false);
  p.putInt("n", n_);
  char key[8];
  for (int i = 0; i < n_; ++i) {
    snprintf(key, sizeof(key), "%d", i);
    p.putString(key, names_[i]);
  }
  p.end();
}

bool IgnoreList::contains(const char* name) const {
  if (!name || !name[0]) return false;
  Lock lk;
  for (int i = 0; i < n_; ++i)
    if (strcmp(names_[i], name) == 0) return true;
  return false;
}

bool IgnoreList::add(const char* name) {
  if (!name || !name[0]) return false;
  Lock lk;
  if (n_ >= MAXN) return false;
  for (int i = 0; i < n_; ++i)
    if (strcmp(names_[i], name) == 0) return false;  // dup
  strncpy(names_[n_], name, NAMELEN - 1);
  names_[n_][NAMELEN - 1] = 0;
  ++n_;
  save();
  return true;
}

bool IgnoreList::removeAt(int i) {
  Lock lk;
  if (i < 0 || i >= n_) return false;
  for (int j = i; j < n_ - 1; ++j) memcpy(names_[j], names_[j + 1], NAMELEN);
  --n_;
  save();
  return true;
}

int IgnoreList::count() const {
  Lock lk;
  return n_;
}

bool IgnoreList::nameAt(int i, char* out, int outSize) const {
  Lock lk;
  if (i < 0 || i >= n_) return false;
  strncpy(out, names_[i], outSize - 1);
  out[outSize - 1] = 0;
  return true;
}
