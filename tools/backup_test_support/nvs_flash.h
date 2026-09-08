#pragma once
#define ESP_OK 0
inline int nvs_flash_init_partition(const char *) { return ESP_OK; }
