#pragma once
#include <cstdlib>
inline void esp_fill_random(void *data, size_t size) { arc4random_buf(data, size); }
