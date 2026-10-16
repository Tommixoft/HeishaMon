#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#include "parsenum.h"

static bool is_arg_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool parse_int_arg(const char *msg, long min, long max, long *out) {
  if (msg == NULL) {
    return false;
  }

  const char *p = msg;
  while (is_arg_space(*p)) {
    p++;
  }

  const char *number = p;
  if (*p == '+' || *p == '-') {
    p++;
  }
  if (!isdigit((unsigned char)*p)) {
    return false;
  }
  while (isdigit((unsigned char)*p)) {
    p++;
  }
  const char *number_end = p;

  if (*p == '.' || *p == ',') {
    p++;
    while (isdigit((unsigned char)*p)) {
      p++;
    }
  }
  if (*p != '\0' && !is_arg_space(*p)) {
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
