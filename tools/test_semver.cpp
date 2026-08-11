#include <cassert>

#include "../WaveshareHodiny/SemVer.h"

int main() {
  assert(semVerIsValid("1.0.0"));
  assert(semVerIsValid("1.2.0-test.10"));
  assert(semVerIsValid("1.2.0-test.2+build.7"));
  assert(!semVerIsValid("1.0"));
  assert(!semVerIsValid("20260809-162006"));
  assert(!semVerIsValid("01.0.0"));
  assert(!semVerIsValid("1.0.0-test.01"));
  assert(!semVerIsValid("1.0.0-"));

  assert(semVerCompare("1.0.0-test.1", "1.0.0") < 0);
  assert(semVerCompare("1.2.0-test.2", "1.2.0-test.10") < 0);
  assert(semVerCompare("1.2.0-test.10", "1.2.0") < 0);
  assert(semVerCompare("1.2.0", "2.0.0") < 0);
  assert(semVerCompare("1.2.0+first", "1.2.0+second") == 0);
  assert(semVerCompare("1.0.0-alpha", "1.0.0-alpha.1") < 0);
  assert(semVerCompare("1.0.0-alpha.1", "1.0.0-alpha.beta") < 0);
  return 0;
}
