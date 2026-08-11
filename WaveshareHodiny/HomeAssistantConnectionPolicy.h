#pragma once

#include <cstring>

inline bool homeAssistantMayReuseStoredToken(const char *requestedUrl,
                                             const char *storedUrl) {
  if (requestedUrl == nullptr || storedUrl == nullptr) return false;
  return requestedUrl[0] == '\0' || std::strcmp(requestedUrl, storedUrl) == 0;
}
