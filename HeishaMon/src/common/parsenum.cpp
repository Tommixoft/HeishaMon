#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <stdlib.h>

#include "parsenum.h"

static bool is_arg_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/*
  Scans `ws* [+-]? digit+ ([.,] digit*)? (ws+ any*)?`. On success *start points
  at the sign or first digit, *int_end just past the integer digits and *end
  just past the fraction.
*/
static bool scan_number(const char *msg, const char **start, const char **int_end, const char **end) {
  if (msg == NULL) {
    return false;
  }

  const char *p = msg;
  while (is_arg_space(*p)) {
    p++;
  }

  *start = p;
  if (*p == '+' || *p == '-') {
    p++;
  }
  if (!isdigit((unsigned char)*p)) {
    return false;
  }
  while (isdigit((unsigned char)*p)) {
    p++;
  }
  *int_end = p;

  if (*p == '.' || *p == ',') {
    p++;
    while (isdigit((unsigned char)*p)) {
      p++;
    }
  }
  if (*p != '\0' && !is_arg_space(*p)) {
    return false;
  }
  *end = p;
  return true;
}

bool parse_int_arg(const char *msg, long min, long max, long *out) {
  const char *number;
  const char *number_end;
  const char *fraction_end;
  if (!scan_number(msg, &number, &number_end, &fraction_end)) {
    return false;
  }

  errno = 0;
  char *end = NULL;
  long value = strtol(number, &end, 10);
  if (errno == ERANGE || end != number_end || value < min || value > max) {
    return false;
  }

  *out = value;
  return true;
}

bool parse_float_arg(const char *msg, float *out) {
  const char *start;
  const char *int_end;
  const char *end;
  if (!scan_number(msg, &start, &int_end, &end)) {
    return false;
  }

  char number[32];
  size_t len = (size_t)(end - start);
  if (len >= sizeof(number)) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    number[i] = (start[i] == ',') ? '.' : start[i];
  }
  number[len] = '\0';

  errno = 0;
  double value = strtod(number, NULL);
  if (errno == ERANGE || value > FLT_MAX || value < -FLT_MAX) {
    return false;
  }

  *out = (float)value;
  return true;
}
