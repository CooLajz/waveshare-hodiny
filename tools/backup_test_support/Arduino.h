#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <initializer_list>
class String : public std::string {
 public:
  using std::string::string;
  String(const std::string &s) : std::string(s) {}
};
template <typename T, typename L, typename H> T constrain(T v, L lo, H hi) {
  return v < lo ? T(lo) : v > hi ? T(hi) : v;
}
