#include "ClockLvglMemory.h"

#include <esp_heap_caps.h>

namespace {
constexpr size_t PSRAM_ALLOCATION_THRESHOLD = 4096;

uint32_t capabilitiesForSize(size_t size) {
  return size >= PSRAM_ALLOCATION_THRESHOLD
             ? MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
             : MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
}
}  // namespace

extern "C" void *clockLvglAlloc(size_t size) {
  void *pointer = heap_caps_malloc(size, capabilitiesForSize(size));
  if (pointer == nullptr && size < PSRAM_ALLOCATION_THRESHOLD) {
    pointer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  return pointer;
}

extern "C" void *clockLvglRealloc(void *pointer, size_t size) {
  void *resized = heap_caps_realloc(pointer, size, capabilitiesForSize(size));
  if (resized == nullptr && size < PSRAM_ALLOCATION_THRESHOLD) {
    resized = heap_caps_realloc(pointer, size,
                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  return resized;
}

extern "C" void clockLvglFree(void *pointer) { heap_caps_free(pointer); }
