#pragma once

// Zachová kompletní konfiguraci dodanou použitou verzí LVGL a pouze pro tento
// projekt zapne vestavěný dekodér animovaných GIFů.
#include <lv_conf.h>

#undef LV_USE_GIF
#define LV_USE_GIF 1

// Malé objekty zůstávají v rychlé interní SRAM, velké grafické alokace jsou
// směrovány do 8MB OPI PSRAM přes projektový allocator.
#undef LV_MEM_CUSTOM
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE "ClockLvglMemory.h"
#define LV_MEM_CUSTOM_ALLOC clockLvglAlloc
#define LV_MEM_CUSTOM_REALLOC clockLvglRealloc
#define LV_MEM_CUSTOM_FREE clockLvglFree
