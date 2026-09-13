#pragma once

#include <esp_heap_caps.h>
#include <new>

// Long-lived scratch storage: allocate on first use, then reuse without heap
// churn. Each caller keeps its own buffer, including nested load/decode/save.
// Never fall back to internal RAM when PSRAM is exhausted.
template <typename T> class ConfigPsramBuffer {
 public:
  ConfigPsramBuffer() = default;
  ConfigPsramBuffer(const ConfigPsramBuffer &) = delete;
  ConfigPsramBuffer &operator=(const ConfigPsramBuffer &) = delete;

  T *get() {
    if (value_ == nullptr) {
      void *storage = heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (storage != nullptr) value_ = new (storage) T{};
    }
    return value_;
  }

 private:
  T *value_ = nullptr;
};
