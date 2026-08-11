#include <cassert>

#include "../WaveshareHodiny/HomeAssistantConnectionPolicy.h"

int main() {
  assert(homeAssistantMayReuseStoredToken("", "http://homeassistant.local:8123"));
  assert(homeAssistantMayReuseStoredToken("http://homeassistant.local:8123",
                                          "http://homeassistant.local:8123"));
  assert(!homeAssistantMayReuseStoredToken("http://attacker.invalid",
                                           "http://homeassistant.local:8123"));
  assert(!homeAssistantMayReuseStoredToken(nullptr,
                                           "http://homeassistant.local:8123"));
  assert(!homeAssistantMayReuseStoredToken("http://homeassistant.local:8123",
                                           nullptr));
  return 0;
}
