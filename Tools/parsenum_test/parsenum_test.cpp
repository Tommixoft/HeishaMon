#include <math.h>
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

static void expect_float(const char *msg, float expected) {
  float value = 12345.0f;
  if (!parse_float_arg(msg, &value) || fabsf(value - expected) > 0.0001f * fmaxf(1.0f, fabsf(expected))) {
    printf("FAIL accept float '%s': expected %g, got %g\n", msg, expected, value);
    failures++;
  }
}

static void expect_float_reject(const char *msg) {
  float value = 12345.0f;
  if (parse_float_arg(msg, &value) || value != 12345.0f) {
    printf("FAIL reject float '%s'\n", msg == NULL ? "NULL" : msg);
    failures++;
  }
}

static void test_float() {
  expect_float("21.5", 21.5f);
  expect_float("21,5", 21.5f);
  expect_float("-3", -3.0f);
  expect_float(" 7 \xC2\xB0" "C", 7.0f);
  expect_float("+3.25", 3.25f);
  expect_float("-0,5", -0.5f);
  expect_float("7 ", 7.0f);
  expect_float("1.", 1.0f);
  expect_float("1234567890123456789012345678901", 1.2345679e30f);  // 31 characters

  expect_float_reject(NULL);
  expect_float_reject("");
  expect_float_reject("unavailable");
  expect_float_reject("1e3");
  expect_float_reject("nan");
  expect_float_reject("inf");
  expect_float_reject(".5");
  expect_float_reject("21.5abc");
  expect_float_reject("21.5\xC2\xB0" "C");
  expect_float_reject("12345678901234567890123456789012");  // 32 characters
  expect_float_reject("1234567890123456789012345678901234567890");
}

int main() {
  test_float();
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
    printf("%d parse test(s) failed\n", failures);
    return 1;
  }
  printf("parse_int_arg, parse_float_arg: all tests passed\n");
  return 0;
}
