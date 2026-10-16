/*
  Pins the input range and byte encoding of every setter in the commands[]
  and optionalCommands[] tables (HeishaMon/commands.h).

  Each setter is called with NULL, "", min-1, every value in [min, max],
  max+1 and "1.5". A rejected value must return 0 and leave the output
  untouched; an accepted one must return the full packet length and change
  only the setter's byte, to the value given by the encoding below.
*/
#include <stdio.h>
#include <string.h>

#include "commands.h"

static int failures = 0;
static int checks = 0;

#define CMD_SENTINEL 0xA5

// panasonicSendQuery: write header, every payload byte 0x00 ("keep current").
static const unsigned char SEND_TEMPLATE[4] = { 0xf1, 0x6c, 0x01, 0x10 };

static unsigned char send_template(int i) {
  return i < 4 ? SEND_TEMPLATE[i] : 0x00;
}

struct MainSpec {
  const char *name;
  long min;
  long max;
  long step;  // accepted values are min, min+step, ... max
  int index;  // byte written into cmd
  int (*encode)(long value);
};

static const MainSpec MAIN_SPECS[] = {
  { "SetHeatpump", 0, 1, 1, 4, [](long v) { return (int)(v + 1); } },
  { "SetPump", 0, 1, 1, 4, [](long v) { return (int)((v + 1) * 16); } },
  { "SetMaxPumpDuty", 0, 254, 1, 45, [](long v) { return (int)(v + 1); } },
  { "SetQuietMode", 0, 30, 1, 7, [](long v) { return (int)((v + 1) * 8); } },
  { "SetZ1HeatRequestTemperature", -127, 127, 1, 38, [](long v) { return (int)(v + 128); } },
  { "SetZ1CoolRequestTemperature", -127, 127, 1, 39, [](long v) { return (int)(v + 128); } },
  { "SetZ2HeatRequestTemperature", -127, 127, 1, 40, [](long v) { return (int)(v + 128); } },
  { "SetZ2CoolRequestTemperature", -127, 127, 1, 41, [](long v) { return (int)(v + 128); } },
  { "SetForceDHW", 0, 1, 1, 4, [](long v) { return (int)((v + 1) * 64); } },
  { "SetForceDefrost", 0, 1, 1, 8, [](long v) { return (int)(v * 2); } },
  { "SetForceSterilization", 0, 1, 1, 8, [](long v) { return (int)(v * 4); } },
  { "SetSterilizationTemp", 55, 75, 1, 100, [](long v) { return (int)(v + 128); } },
  { "SetSterilizationMaxTime", 5, 60, 5, 101, [](long v) { return (int)(v + 1); } },
  { "SetForceHeater", 0, 1, 1, 5, [](long v) { return (int)((v + 1) * 4); } },
  { "SetHolidayMode", 0, 1, 1, 5, [](long v) { return (int)((v + 1) * 16); } },
  { "SetPowerfulMode", 0, 6, 1, 7, [](long v) { return (int)(v + 1); } },
  { "SetOperationMode", 0, 6, 1, 6, [](long v) {
      static const int modes[] = { 18, 19, 24, 33, 34, 35, 40 };
      return modes[v];
    } },
  { "SetDHWTemp", -127, 127, 1, 42, [](long v) { return (int)(v + 128); } },
  { "SetZones", 0, 2, 1, 6, [](long v) { return (int)((v + 1) * 64); } },
  { "SetFloorHeatDelta", -127, 127, 1, 84, [](long v) { return (int)(v + 128); } },
  { "SetFloorCoolDelta", -127, 127, 1, 94, [](long v) { return (int)(v + 128); } },
  { "SetDHWHeatDelta", -127, 127, 1, 99, [](long v) { return (int)(v + 128); } },
  { "SetReset", 0, 1, 1, 8, [](long v) { return (int)v; } },
  { "SetHeaterDelayTime", 0, 254, 1, 104, [](long v) { return (int)(v + 1); } },
  { "SetHeaterStartDelta", -127, 127, 1, 105, [](long v) { return (int)(v + 128); } },
  { "SetHeaterStopDelta", -127, 127, 1, 106, [](long v) { return (int)(v + 128); } },
  { "SetMainSchedule", 0, 1, 1, 5, [](long v) { return (int)((v + 1) * 64); } },
  { "SetAltExternalSensor", 0, 1, 1, 20, [](long v) { return (int)((v + 1) * 16); } },
  { "SetExternalPadHeater", 0, 2, 1, 25, [](long v) { return (int)((v + 1) * 16); } },
  { "SetBufferDelta", -127, 127, 1, 59, [](long v) { return (int)(v + 128); } },
  { "SetBuffer", 0, 1, 1, 24, [](long v) { return (int)((v + 1) * 4); } },
  { "SetHeatingOffOutdoorTemp", -127, 127, 1, 83, [](long v) { return (int)(v + 128); } },
  { "SetExternalControl", 0, 1, 1, 23, [](long v) { return (int)(v + 1); } },
  { "SetExternalError", 0, 1, 1, 23, [](long v) { return (int)((v + 1) * 16); } },
  { "SetExternalCompressorControl", 0, 1, 1, 23, [](long v) { return (int)((v + 1) * 64); } },
  { "SetExternalHeatCoolControl", 0, 1, 1, 23, [](long v) { return (int)((v + 1) * 4); } },
  { "SetBivalentControl", 0, 1, 1, 26, [](long v) { return (int)(v + 1); } },
  { "SetBivalentMode", 0, 2, 1, 26, [](long v) { return (int)((v + 1) * 4); } },
  { "SetBivalentStartTemp", -127, 127, 1, 65, [](long v) { return (int)(v + 128); } },
  { "SetBivalentAPStartTemp", -127, 127, 1, 66, [](long v) { return (int)(v + 128); } },
  { "SetBivalentAPStopTemp", -127, 127, 1, 68, [](long v) { return (int)(v + 128); } },
  { "SetHeatingControl", 0, 1, 1, 30, [](long v) { return (int)((v + 1) << 2); } },
  { "SetSmartDHW", 0, 1, 1, 24, [](long v) { return (int)((v + 1) << 6); } },
  { "SetQuietModePriority", 0, 1, 1, 11, [](long v) { return (int)((v + 1) << 4); } },
  { "SetPumpFlowrateMode", 0, 1, 1, 29, [](long v) { return (int)((v + 1) << 4); } },
  { "SetDHWSensorSelection", 0, 1, 1, 11, [](long v) { return (int)(v + 1); } },
  { "SetDHWHeaterState", 0, 1, 1, 9, [](long v) { return (int)((v + 1) << 2); } },
  { "SetRoomHeaterState", 0, 1, 1, 9, [](long v) { return (int)(v + 1); } },
  { "SetHeaterOnOutdoorTemp", -127, 127, 1, 85, [](long v) { return (int)(v + 128); } },
};

// Optional PCB setters change bits (mask << shift) of optionalPCBQuery[index] to the value.
struct OptSpec {
  const char *name;
  long min;
  long max;
  int index;
  int mask;
  int shift;
};

static const OptSpec OPT_SPECS[] = {
  { "SetHeatCoolMode", 0, 1, 6, 0b1, 7 },
  { "SetCompressorState", 0, 1, 6, 0b1, 6 },
  { "SetSmartGridMode", 0, 3, 6, 0b11, 4 },
  { "SetExternalThermostat1State", 0, 3, 6, 0b11, 2 },
  { "SetExternalThermostat2State", 0, 3, 6, 0b11, 0 },
  { "SetDemandControl", 0, 255, 14, 0xFF, 0 },
  { "SetOptPCBByte9", 0, 255, 9, 0xFF, 0 },
};

// Optional PCB temperature setters (set_xxx_temp) keep their unvalidated toFloat() parsing.
struct OptTempSpec {
  const char *name;
  int index;
};

static const OptTempSpec OPT_TEMP_SPECS[] = {
  { "SetPoolTemp", 7 },
  { "SetBufferTemp", 8 },
  { "SetZ1RoomTemp", 10 },
  { "SetZ1WaterTemp", 16 },
  { "SetZ2RoomTemp", 11 },
  { "SetZ2WaterTemp", 15 },
  { "SetSolarTemp", 13 },
};

static void fail(const char *name, const char *msg, const char *what) {
  printf("FAIL %s('%s'): %s\n", name, msg == NULL ? "NULL" : msg, what);
  failures++;
}

static unsigned int (*find_main(const char *name))(char *, unsigned char *, char *) {
  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    if (strcmp(commands[i].name, name) == 0) {
      return commands[i].func;
    }
  }
  return NULL;
}

static unsigned int (*find_opt(const char *name))(char *, char *) {
  for (unsigned int i = 0; i < sizeof(optionalCommands) / sizeof(optionalCommands[0]); i++) {
    if (strcmp(optionalCommands[i].name, name) == 0) {
      return optionalCommands[i].func;
    }
  }
  return NULL;
}

static void check_log(const char *name, const char *msg, const char *log_msg) {
  if (log_msg[0] == '\0' || memchr(log_msg, '\0', 256) == NULL) {
    fail(name, msg, "log message empty or not terminated");
  }
}

// Calls a main setter; cmd is pre-filled with CMD_SENTINEL.
static unsigned int call_main(unsigned int (*func)(char *, unsigned char *, char *),
                              const char *msg, unsigned char *cmd, char *log_msg) {
  char buf[512];
  memset(cmd, CMD_SENTINEL, 256);
  memset(log_msg, 0, 256);
  if (msg != NULL) {
    snprintf(buf, sizeof(buf), "%s", msg);
  }
  checks++;
  return func(msg == NULL ? NULL : buf, cmd, log_msg);
}

static void expect_main_reject(const char *name, const char *msg) {
  unsigned char cmd[256];
  char log_msg[256];
  unsigned int len = call_main(find_main(name), msg, cmd, log_msg);
  if (len != 0) {
    fail(name, msg, "expected rejection");
    return;
  }
  for (int i = 0; i < 256; i++) {
    if (cmd[i] != CMD_SENTINEL) {
      fail(name, msg, "rejected value wrote into cmd");
      return;
    }
  }
  check_log(name, msg, log_msg);
}

// changes lists {index, value} pairs, terminated by index -1; all other bytes keep the template.
static void expect_main_packet(const char *name, const char *msg, const int changes[][2]) {
  unsigned char cmd[256];
  char log_msg[256];
  unsigned int len = call_main(find_main(name), msg, cmd, log_msg);
  if (len != PANASONICQUERYSIZE) {
    char what[64];
    snprintf(what, sizeof(what), "returned %u, expected %d", len, PANASONICQUERYSIZE);
    fail(name, msg, what);
    return;
  }
  for (int i = 0; i < 256; i++) {
    int expected = i < PANASONICQUERYSIZE ? send_template(i) : CMD_SENTINEL;
    for (int c = 0; changes[c][0] >= 0; c++) {
      if (changes[c][0] == i) {
        expected = changes[c][1];
      }
    }
    if (cmd[i] != expected) {
      char what[64];
      snprintf(what, sizeof(what), "cmd[%d] = 0x%02x, expected 0x%02x", i, cmd[i], expected);
      fail(name, msg, what);
      return;
    }
  }
  check_log(name, msg, log_msg);
}

static void expect_main_value(const MainSpec &spec, const char *msg, long value) {
  const int changes[][2] = { { spec.index, spec.encode(value) & 0xFF }, { -1, 0 } };
  expect_main_packet(spec.name, msg, changes);
}

static bool main_accepts(const MainSpec &spec, long value) {
  return value >= spec.min && value <= spec.max && (value - spec.min) % spec.step == 0;
}

static void test_main_spec(const MainSpec &spec) {
  if (find_main(spec.name) == NULL) {
    fail(spec.name, "", "not in commands[]");
    return;
  }
  char msg[32];

  expect_main_reject(spec.name, NULL);
  expect_main_reject(spec.name, "");
  for (long v = spec.min - 1; v <= spec.max + 1; v++) {
    snprintf(msg, sizeof(msg), "%ld", v);
    if (main_accepts(spec, v)) {
      expect_main_value(spec, msg, v);
    } else {
      expect_main_reject(spec.name, msg);
    }
  }

  // Fractions truncate to 1 in parse_int_arg; the sterilization setters reject them.
  bool fraction_ok = strncmp(spec.name, "SetSterilization", 16) != 0;
  if (fraction_ok && main_accepts(spec, 1)) {
    expect_main_value(spec, "1.5", 1);
  } else {
    expect_main_reject(spec.name, "1.5");
  }
}

static void test_curves() {
  static const char *name = "SetCurves";
  if (find_main(name) == NULL) {
    fail(name, "", "not in commands[]");
    return;
  }

  const char *full =
    "{\"zone1\":{\"heat\":{\"target\":{\"high\":1,\"low\":2},\"outside\":{\"low\":3,\"high\":4}},"
    "\"cool\":{\"target\":{\"high\":9,\"low\":10},\"outside\":{\"low\":11,\"high\":12}}},"
    "\"zone2\":{\"heat\":{\"target\":{\"high\":5,\"low\":6},\"outside\":{\"low\":7,\"high\":8}},"
    "\"cool\":{\"target\":{\"high\":13,\"low\":14},\"outside\":{\"low\":15,\"high\":-16}}}}";
  const int full_changes[][2] = {
    { 75, 129 }, { 76, 130 }, { 77, 131 }, { 78, 132 },
    { 79, 133 }, { 80, 134 }, { 81, 135 }, { 82, 136 },
    { 86, 137 }, { 87, 138 }, { 88, 139 }, { 89, 140 },
    { 90, 141 }, { 91, 142 }, { 92, 143 }, { 93, 112 }, { -1, 0 }
  };
  expect_main_packet(name, full, full_changes);

  // Missing keys keep 0x00; numbers truncate toward zero; numeric strings are accepted.
  const int z1_high_min[][2] = { { 75, 1 }, { -1, 0 } };
  expect_main_packet(name, "{\"zone1\":{\"heat\":{\"target\":{\"high\":-127}}}}", z1_high_min);
  const int z1_high_max[][2] = { { 75, 255 }, { -1, 0 } };
  expect_main_packet(name, "{\"zone1\":{\"heat\":{\"target\":{\"high\":127.9}}}}", z1_high_max);
  const int z2_cool_low[][2] = { { 92, 1 }, { -1, 0 } };
  expect_main_packet(name, "{\"zone2\":{\"cool\":{\"outside\":{\"low\":-127.9}}}}", z2_cool_low);
  const int z1_cool_str[][2] = { { 86, 149 }, { -1, 0 } };
  expect_main_packet(name, "{\"zone1\":{\"cool\":{\"target\":{\"high\":\"21\"}}}}", z1_cool_str);
  const int null_key[][2] = { { -1, 0 } };
  expect_main_packet(name, "{\"zone1\":{\"heat\":{\"target\":{\"high\":null}}}}", null_key);
  // Valid JSON that is not an object has no curve keys, so nothing changes.
  expect_main_packet(name, "1.5", null_key);

  // Rejections return 0 (cmd may be partly written, it is not sent).
  const char *rejects[] = {
    NULL, "", "{", "not json",
    "{\"zone1\":{\"heat\":{\"target\":{\"high\":128}}}}",
    "{\"zone1\":{\"heat\":{\"target\":{\"high\":-128}}}}",
    "{\"zone2\":{\"cool\":{\"outside\":{\"high\":1e3}}}}",
    "{\"zone1\":{\"heat\":{\"target\":{\"high\":\"abc\"}}}}",
    "{\"zone1\":{\"heat\":{\"target\":{\"high\":true}}}}",
  };
  for (unsigned int i = 0; i < sizeof(rejects) / sizeof(rejects[0]); i++) {
    unsigned char cmd[256];
    char log_msg[256];
    if (call_main(find_main(name), rejects[i], cmd, log_msg) != 0) {
      fail(name, rejects[i], "expected rejection");
    } else {
      check_log(name, rejects[i], log_msg);
    }
  }
}

static unsigned int call_opt(unsigned int (*func)(char *, char *), const char *msg, char *log_msg) {
  char buf[64];
  memset(log_msg, 0, 256);
  if (msg != NULL) {
    snprintf(buf, sizeof(buf), "%s", msg);
  }
  checks++;
  return func(msg == NULL ? NULL : buf, log_msg);
}

// Runs one optional setter call on a known optionalPCBQuery and checks the result.
// expected < 0 means the call must be rejected and change nothing.
static void expect_opt(const char *name, const char *msg, int index, int background, int expected) {
  unsigned char before[OPTIONALPCBQUERYSIZE];
  char log_msg[256];
  optionalPCBQuery[index] = background;
  memcpy(before, optionalPCBQuery, sizeof(before));

  unsigned int len = call_opt(find_opt(name), msg, log_msg);
  unsigned int expected_len = expected < 0 ? 0 : OPTIONALPCBQUERYSIZE;
  if (len != expected_len) {
    char what[64];
    snprintf(what, sizeof(what), "returned %u, expected %u", len, expected_len);
    fail(name, msg, what);
    return;
  }
  for (int i = 0; i < OPTIONALPCBQUERYSIZE; i++) {
    int want = (i == index && expected >= 0) ? expected : before[i];
    if (optionalPCBQuery[i] != want) {
      char what[64];
      snprintf(what, sizeof(what), "optionalPCBQuery[%d] = 0x%02x, expected 0x%02x", i, optionalPCBQuery[i], want);
      fail(name, msg, what);
      return;
    }
  }
  check_log(name, msg, log_msg);
}

static void test_opt_spec(const OptSpec &spec) {
  if (find_opt(spec.name) == NULL) {
    fail(spec.name, "", "not in optionalCommands[]");
    return;
  }
  static const int backgrounds[] = { 0x00, 0xFF, 0x5A };
  char msg[32];

  for (unsigned int b = 0; b < sizeof(backgrounds) / sizeof(backgrounds[0]); b++) {
    int bg = backgrounds[b];
    int keep = bg & ~(spec.mask << spec.shift) & 0xFF;

    expect_opt(spec.name, NULL, spec.index, bg, -1);
    expect_opt(spec.name, "", spec.index, bg, -1);
    for (long v = spec.min - 1; v <= spec.max + 1; v++) {
      snprintf(msg, sizeof(msg), "%ld", v);
      bool accepted = v >= spec.min && v <= spec.max;
      expect_opt(spec.name, msg, spec.index, bg, accepted ? (keep | (int)(v << spec.shift)) : -1);
    }
    expect_opt(spec.name, "1.5", spec.index, bg, keep | (1 << spec.shift));
  }
}

static void test_opt_temp_spec(const OptTempSpec &spec) {
  if (find_opt(spec.name) == NULL) {
    fail(spec.name, "", "not in optionalCommands[]");
    return;
  }
  // NTC encoding: 25 C is R25, so 255 * 6340 / (6480 + 6340) = 126; out of range clamps.
  expect_opt(spec.name, "25", spec.index, 0x00, 126);
  expect_opt(spec.name, "121", spec.index, 0x00, 0);
  expect_opt(spec.name, "-79", spec.index, 0x00, 255);

  // Unvalidated: anything toFloat() reads as 0 encodes like "0".
  char log_msg[256];
  call_opt(find_opt(spec.name), "0", log_msg);
  int zero = optionalPCBQuery[spec.index];
  expect_opt(spec.name, NULL, spec.index, 0x00, zero);
  expect_opt(spec.name, "", spec.index, 0x00, zero);
  expect_opt(spec.name, "abc", spec.index, 0x00, zero);
}

// Every table entry must have a spec, so a new setter cannot slip in untested.
static void test_table_coverage() {
  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    bool found = strcmp(commands[i].name, "SetCurves") == 0;
    for (unsigned int s = 0; !found && s < sizeof(MAIN_SPECS) / sizeof(MAIN_SPECS[0]); s++) {
      found = strcmp(commands[i].name, MAIN_SPECS[s].name) == 0;
    }
    if (!found) {
      fail(commands[i].name, "", "commands[] entry has no spec in commands_test.cpp");
    }
  }
  for (unsigned int i = 0; i < sizeof(optionalCommands) / sizeof(optionalCommands[0]); i++) {
    bool found = false;
    for (unsigned int s = 0; !found && s < sizeof(OPT_SPECS) / sizeof(OPT_SPECS[0]); s++) {
      found = strcmp(optionalCommands[i].name, OPT_SPECS[s].name) == 0;
    }
    for (unsigned int s = 0; !found && s < sizeof(OPT_TEMP_SPECS) / sizeof(OPT_TEMP_SPECS[0]); s++) {
      found = strcmp(optionalCommands[i].name, OPT_TEMP_SPECS[s].name) == 0;
    }
    if (!found) {
      fail(optionalCommands[i].name, "", "optionalCommands[] entry has no spec in commands_test.cpp");
    }
  }
}

int main() {
  test_table_coverage();
  for (unsigned int s = 0; s < sizeof(MAIN_SPECS) / sizeof(MAIN_SPECS[0]); s++) {
    test_main_spec(MAIN_SPECS[s]);
  }
  test_curves();
  for (unsigned int s = 0; s < sizeof(OPT_SPECS) / sizeof(OPT_SPECS[0]); s++) {
    test_opt_spec(OPT_SPECS[s]);
  }
  for (unsigned int s = 0; s < sizeof(OPT_TEMP_SPECS) / sizeof(OPT_TEMP_SPECS[0]); s++) {
    test_opt_temp_spec(OPT_TEMP_SPECS[s]);
  }

  // Extra cases that the range sweep does not reach.
  expect_main_reject("SetSterilizationTemp", "55.5");
  expect_main_reject("SetSterilizationTemp", " 55");
  expect_main_reject("SetSterilizationMaxTime", "5.0");
  expect_main_reject("SetSterilizationMaxTime", "100");
  expect_main_reject("SetQuietMode", "ON");
  expect_main_reject("SetZ1HeatRequestTemperature", "21abc");
  const int dhw_fraction[][2] = { { 42, 49 + 128 }, { -1, 0 } };
  expect_main_packet("SetDHWTemp", "49.9", dhw_fraction);

  if (failures) {
    printf("%d of %d setter checks failed\n", failures, checks);
    return 1;
  }
  printf("all %d setter checks passed\n", checks);
  return 0;
}
