// IgnoreList: names (advertised BLE names / SSIDs) the user never wants to hear
// about — their own gear. Read from the scan task (contains) and written from
// the UI task (add/removeAt), so access is mutex-guarded. Persisted in NVS.
#pragma once
#include <stdint.h>

class IgnoreList {
 public:
  void begin();  // create mutex + load from NVS

  bool contains(const char* name) const;  // scan-task safe
  bool add(const char* name);              // UI task; false if full/dup/empty
  bool removeAt(int i);                    // UI task
  int count() const;
  // Copy entry i into out (UI task). Returns false if out of range.
  bool nameAt(int i, char* out, int outSize) const;

 private:
  static constexpr int MAXN = 16;
  static constexpr int NAMELEN = 24;
  char names_[MAXN][NAMELEN] = {};
  int n_ = 0;
  void save();
};

extern IgnoreList ignores;
