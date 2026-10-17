#include <stdlib.h>

#include "description.h"

const char *getDescription(const char *const *description, int value, const char *unknown) {
  int count = atoi(description[0]);
  if (count == 0) {
    return description[1];
  }
  if (value < 0 || value >= count) {
    return unknown;
  }
  return description[value + 1];
}
