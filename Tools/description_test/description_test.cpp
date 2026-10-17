#include <stdio.h>
#include <string.h>

#include "../../HeishaMon/src/common/description.h"

static const char unknown[] = "unknown";
static const char *celsius[] = {"0", "°C"};
static const char *disabledEnabled[] = {"2", "Disabled", "Enabled"};

static int failures = 0;

static void expect(const char *const *description, int value, const char *expected) {
  const char *actual = getDescription(description, value, unknown);
  if (actual == NULL || strcmp(actual, expected) != 0) {
    printf("FAIL table '%s' value %d: expected '%s', got '%s'\n", description[0], value, expected, actual == NULL ? "NULL" : actual);
    failures++;
  }
}

int main() {
  expect(celsius, 0, "°C");
  expect(celsius, 5, "°C");
  expect(celsius, -1, "°C");

  expect(disabledEnabled, -1, "unknown");
  expect(disabledEnabled, 0, "Disabled");
  expect(disabledEnabled, 1, "Enabled");
  expect(disabledEnabled, 2, "unknown");
  expect(disabledEnabled, 99, "unknown");

  if (failures > 0) {
    printf("%d getDescription test(s) failed\n", failures);
    return 1;
  }
  printf("getDescription: all tests passed\n");
  return 0;
}
