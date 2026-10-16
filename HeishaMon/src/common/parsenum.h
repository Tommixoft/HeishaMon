#ifndef PARSENUM_H
#define PARSENUM_H

/*
  Parses a command argument (MQTT payload, web form value or rules value)
  as a whole number within [min, max].

  Accepted: optional surrounding whitespace, an optional sign, digits, an
  optional '.' or ',' fraction that is truncated toward zero ("21.5" -> 21),
  and optional trailing text after whitespace, such as a unit ("50 °C").

  Rejected: NULL, empty input, anything not starting with a digit
  ("ON", "unavailable", "nan"), text glued to the digits ("21abc",
  "1e+06", "0x10") and values outside [min, max].

  *out is written only when true is returned.
*/
bool parse_int_arg(const char *msg, long min, long max, long *out);

#endif
