#pragma once

// Samostatná projektová konfigurace pro LVGL 8.3.x. Záměrně se neopírá o
// ručně vytvořený lv_conf.h uvnitř lokální instalace knihovny, aby čistý
// checkout používal stejné volby na každém počítači.
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_GIF 1

// Malé objekty zůstávají v rychlé interní SRAM, velké grafické alokace jsou
// směrovány do 8MB OPI PSRAM přes projektový allocator.
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE "ClockLvglMemory.h"
#define LV_MEM_CUSTOM_ALLOC clockLvglAlloc
#define LV_MEM_CUSTOM_REALLOC clockLvglRealloc
#define LV_MEM_CUSTOM_FREE clockLvglFree
