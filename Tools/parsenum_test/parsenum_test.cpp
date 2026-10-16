#include <stdio.h>

#include "../../HeishaMon/src/common/parsenum.h"

static int failures = 0;

static void expect_value(const char *msg, long min, long max, long expected) {
  long value = 12345;
  if (!parse_int_arg(msg, min, max, &value) || value != expected) {
    printf("FAIL accept '%s' [%ld,%ld]: expected %ld\n", msg, min, max, expected);
    failures++;
  }
}

static void expect_reject(const char *msg, long min, long max) {
  long value = 12345;
  if (parse_int_arg(msg, min, max, &value) || value != 12345) {
    printf("FAIL reject '%s' [%ld,%ld]\n", msg == NULL ? "NULL" : msg, min, max);
    failures++;
  }
}

int main() {
  expect_value("21", -127, 127, 21);
  expect_value(" 21\n", -127, 127, 21);
  expect_value("\t21\r\n", -127, 127, 21);
  expect_value("+21", -127, 127, 21);
  expect_value("21.", -127, 127, 21);
  expect_value("21.5", -127, 127, 21);
  expect_value("21,5", -127, 127, 21);
  expect_value("-21.9", -127, 127, -21);
  expect_value("-0.5", -127, 127, 0);
  expect_value("50 \xC2\xB0" "C", -127, 127, 50);
  expect_value("-127", -127, 127, -127);
  expect_value("127", -127, 127, 127);
  expect_value("0", 0, 1, 0);
  expect_value("1", 0, 1, 1);
  expect_value("254", 0, 254, 254);

  expect_reject(NULL, -127, 127);
  expect_reject("", -127, 127);
  expect_reject(" ", -127, 127);
  expect_reject("-", -127, 127);
  expect_reject(".5", -127, 127);
  expect_reject("ON", 0, 1);
  expect_reject("unavailable", -127, 127);
  expect_reject("nan", -127, 127);
  expect_reject("1e+06", -127, 127);
  expect_reject("1e-05", -127, 127);
  expect_reject("0x10", 0, 255);
  expect_reject("21abc", -127, 127);
  expect_reject("21.5abc", -127, 127);
  expect_reject("-128", -127, 127);
  expect_reject("128", -127, 127);
  expect_reject("2", 0, 1);
  expect_reject("-1", 0, 3);
  expect_reject("255", 0, 254);
  expect_reject("99999999999999999999", -127, 127);
  expect_reject("-99999999999999999999", -127, 127);

  if (failures > 0) {
    printf("%d parse_int_arg test(s) failed\n", failures);
    return 1;
  }
  printf("parse_int_arg: all tests passed\n");
  return 0;
}
