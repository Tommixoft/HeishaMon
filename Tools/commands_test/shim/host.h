/* Host shim so HeishaMon/commands.cpp compiles off-device.
   Force-included before every translation unit (g++ -include). */
#ifndef _COMMANDS_TEST_HOST_H_
#define _COMMANDS_TEST_HOST_H_

#include "../../../Examples/Rules/harness/shim/Arduino.h"

#include <math.h>
#include <string>

typedef uint8_t byte;
typedef const char *PGM_P;

#define PROGMEM
#define strncpy_P strncpy

/* Only what commands.cpp uses: construction from a C string and toFloat(),
   which is atof() on both ESP cores. A NULL pointer gives an empty string. */
class String {
 public:
  String(const char *cstr) : value(cstr == NULL ? "" : cstr) {}
  float toFloat() const { return atof(value.c_str()); }

 private:
  std::string value;
};

#endif
