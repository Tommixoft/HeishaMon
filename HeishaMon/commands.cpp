#include "commands.h"
#include "src/common/parsenum.h"
#include <LittleFS.h>

//removed checksum from default query, is calculated in send_command
byte initialQuery[] = {0x31, 0x05, 0x10, 0x01, 0x00, 0x00, 0x00};
byte panasonicQuery[] = {0x71, 0x6c, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
byte optionalPCBQuery[] = {0xF1, 0x11, 0x01, 0x50, 0x00, 0x00, 0x40, 0xFF, 0xFF, 0xE5, 0xFF, 0xFF, 0x00, 0xFF, 0xEB, 0xFF, 0xFF, 0x00, 0x00};
byte panasonicSendQuery[] PROGMEM = {0xf1, 0x6c, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#ifdef ESP32
extern QueueHandle_t pcbQueue;
#endif

const char* mqtt_topic_values PROGMEM = "main";
const char* mqtt_topic_xvalues PROGMEM = "extra";
const char* mqtt_topic_commands PROGMEM = "commands";
const char* mqtt_topic_pcbvalues PROGMEM = "optional";
const char* mqtt_topic_1wire PROGMEM = "1wire";
const char* mqtt_topic_s0 PROGMEM = "s0";
const char* mqtt_logtopic PROGMEM = "log";

const char* mqtt_willtopic PROGMEM = "LWT";
const char* mqtt_iptopic PROGMEM = "ip";

const char* mqtt_send_raw_value_topic PROGMEM = "SendRawValue";

static unsigned int temp2hex(float temp) {
  int hextemp = 0;
  if (temp > 120) {
    hextemp = 0;
  } else if (temp < -78) {
    hextemp = 255;
  } else {
    byte Uref = 255;
    int constant = 3695;
    int R25 = 6340;
    byte T25 = 25;
    int Rf = 6480;
    float K = 273.15;
    float RT = R25 * exp(constant * (1 / (temp + K) - 1 / (T25 + K)));
    hextemp = Uref * (RT / (Rf + RT));
  }
  return hextemp;
}

// name is a PSTR command name; it is copied to RAM because %s cannot read flash on ESP8266.
static bool parse_command_arg(const char *msg, long min, long max, long *value, char *log_msg, PGM_P name) {
  if (parse_int_arg(msg, min, max, value)) {
    return true;
  }
  char name_ram[29] = { 0 };
  strncpy_P(name_ram, name, sizeof(name_ram) - 1);
  snprintf_P(log_msg, 256, PSTR("Invalid value '%.32s' for %s: expected integer %ld to %ld"), msg == NULL ? "NULL" : msg, name_ram, min, max);
  return false;
}

unsigned int set_heatpump_state(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 1, &value, log_msg, PSTR("SetHeatpump"))) {
    return 0;
  }

  byte heatpump_state = 1;
  if ( value == 1 ) {
    heatpump_state = 2;
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set heatpump state to %d"), heatpump_state - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[4] = heatpump_state;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_pump(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 1, &value, log_msg, PSTR("SetPump"))) {
    return 0;
  }

  byte pump_state = 16;
  if ( value == 1 ) {
    pump_state = 32;
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set pump state to %d"), (pump_state / 16) - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[4] = pump_state;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_max_pump_duty(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 254, &value, log_msg, PSTR("SetMaxPumpDuty"))) {
    return 0;
  }

  byte pumpduty = value + 1;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set max pump duty to %d"), pumpduty - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[45] = pumpduty;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_quiet_mode(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 30, &value, log_msg, PSTR("SetQuietMode"))) {
    return 0;
  }

  byte quiet_mode = (value + 1) * 8;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set Quiet mode to %d"), quiet_mode / 8 - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[7] = quiet_mode;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_z1_heat_request_temperature(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetZ1HeatRequestTemperature"))) {
    return 0;
  }

  byte request_temp = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set z1 heat request temperature to %d"), request_temp - 128 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[38] = request_temp;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_z1_cool_request_temperature(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetZ1CoolRequestTemperature"))) {
    return 0;
  }

  byte request_temp = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set z1 cool request temperature to %d"), request_temp - 128 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[39] = request_temp;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_z2_heat_request_temperature(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetZ2HeatRequestTemperature"))) {
    return 0;
  }

  byte request_temp = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set z2 heat request temperature to %d"), request_temp - 128 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[40] = request_temp;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_z2_cool_request_temperature(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetZ2CoolRequestTemperature"))) {
    return 0;
  }

  byte request_temp = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set z2 cool request temperature to %d"), request_temp - 128 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[41] = request_temp;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_bivalent_start_temp(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetBivalentStartTemp"))) {
    return 0;
  }

  byte request_temp = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set bivalent start temperature to %d"), request_temp - 128 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[65] = request_temp;
  }

  return sizeof(panasonicSendQuery);
}
unsigned int set_bivalent_ap_start_temp(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetBivalentAPStartTemp"))) {
    return 0;
  }

  byte request_temp = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set bivalent ap start temperature to %d"), request_temp - 128 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[66] = request_temp;
  }

  return sizeof(panasonicSendQuery);
}
unsigned int set_bivalent_ap_stop_temp(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetBivalentAPStopTemp"))) {
    return 0;
  }

  byte request_temp = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set bivalent stop ap temperature to %d"), request_temp - 128 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[68] = request_temp;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_force_DHW(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 1, &value, log_msg, PSTR("SetForceDHW"))) {
    return 0;
  }

  byte force_DHW_mode = 64; //hex 0x40
  if ( value == 1 ) {
    force_DHW_mode = 128; //hex 0x80
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set force DHW mode to %d"), (force_DHW_mode / 64) - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[4] = force_DHW_mode;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_force_defrost(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 1, &value, log_msg, PSTR("SetForceDefrost"))) {
    return 0;
  }

  byte force_defrost_mode = 0;
  if ( value == 1 ) {
    force_defrost_mode = 2; //hex 0x02
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set force defrost mode to %d"), force_defrost_mode / 2);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[8] = force_defrost_mode;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_sterilization_temp(char *msg, unsigned char *cmd, char *log_msg) {
  if (msg == NULL || strlen(msg) != 2 || msg[0] < '0' || msg[0] > '9' || msg[1] < '0' || msg[1] > '9') {
    snprintf_P(log_msg, 256, PSTR("Invalid sterilization temperature: expected integer 55-75 C"));
    return 0;
  }
  const int temperature = (msg[0] - '0') * 10 + (msg[1] - '0');
  if (temperature < 55 || temperature > 75) {
    snprintf_P(log_msg, 256, PSTR("Invalid sterilization temperature: expected integer 55-75 C"));
    return 0;
  }

  memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
  cmd[100] = temperature + 128;
  snprintf_P(log_msg, 256, PSTR("set sterilization temperature to %d C"), temperature);
  return sizeof(panasonicSendQuery);
}

unsigned int set_sterilization_max_time(char *msg, unsigned char *cmd, char *log_msg) {
  int minutes = 0;
  const size_t length = msg == NULL ? 0 : strlen(msg);
  if (length > 0 && length <= 2) {
    for (size_t i = 0; i < length; ++i) {
      if (msg[i] < '0' || msg[i] > '9') {
        minutes = 0;
        break;
      }
      minutes = minutes * 10 + (msg[i] - '0');
    }
  }
  if (minutes < 5 || minutes > 60 || minutes % 5 != 0) {
    snprintf_P(log_msg, 256, PSTR("Invalid sterilization holding time: expected multiple of 5 minutes from 5 to 60"));
    return 0;
  }

  memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
  cmd[101] = minutes + 1;
  snprintf_P(log_msg, 256, PSTR("set sterilization holding time to %d minutes"), minutes);
  return sizeof(panasonicSendQuery);
}

unsigned int set_force_sterilization(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 1, &value, log_msg, PSTR("SetForceSterilization"))) {
    return 0;
  }

  byte force_sterilization_mode = 0;
  if ( value == 1 ) {
    force_sterilization_mode = 4; //hex 0x04
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set force sterilization mode to %d"), force_sterilization_mode / 4);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[8] = force_sterilization_mode;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_force_heater(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 1, &value, log_msg, PSTR("SetForceHeater"))) {
    return 0;
  }

  byte force_heater_mode = 4; //hex 0x04
  if ( value == 1 ) {
    force_heater_mode = 8; //hex 0x08
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set force heater mode to %d"), (force_heater_mode / 4) - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[5] = force_heater_mode;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_holiday_mode(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 1, &value, log_msg, PSTR("SetHolidayMode"))) {
    return 0;
  }

  byte set_holiday = 16; //hex 0x10
  if ( value == 1 ) {
    set_holiday = 32; //hex 0x20
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set holiday mode to %d"), (set_holiday / 16) - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[5] = set_holiday;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_powerful_mode(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 6, &value, log_msg, PSTR("SetPowerfulMode"))) {
    return 0;
  }

  byte set_powerful = value + 1;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set powerful mode to %d"), set_powerful - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[7] = set_powerful;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_DHW_temp(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetDHWTemp"))) {
    return 0;
  }

  byte set_DHW_temp = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set DHW temperature to %d"), set_DHW_temp - 128);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[42] = set_DHW_temp;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_operation_mode(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 6, &value, log_msg, PSTR("SetOperationMode"))) {
    return 0;
  }

  byte set_mode;
  switch (value) {
    case 0: set_mode = 18; break;
    case 1: set_mode = 19; break;
    case 2: set_mode = 24; break;
    case 3: set_mode = 33; break;
    case 4: set_mode = 34; break;
    case 5: set_mode = 35; break;
    case 6: set_mode = 40; break;
    default: set_mode = 0; break;
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set heat pump mode to %d"), set_mode);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[6] = set_mode;
  }

  return sizeof(panasonicSendQuery);
}


unsigned int set_bivalent_control(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 1, &value, log_msg, PSTR("SetBivalentControl"))) {
    return 0;
  }

  byte set_bcontrol = 1;
  if ( value == 1 ) {
    set_bcontrol = 2;
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set bivalent control to %d"), set_bcontrol - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[26] = set_bcontrol;
  }

  return sizeof(panasonicSendQuery);
}


unsigned int set_bivalent_mode(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 2, &value, log_msg, PSTR("SetBivalentMode"))) {
    return 0;
  }

  byte set_bmode = 4; // alternative mode
  if ( value == 1 ) { //parallel mode
    set_bmode = 8;
  }
  if ( value == 2 ) { //advanced parallel mode
    set_bmode = 12;
  }
  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set bivalent mode to %d"), (set_bmode / 4) - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[26] = set_bmode;
  }

  return sizeof(panasonicSendQuery);
}

// A missing or null key keeps the default 0x00 byte, which keeps the current setting for that byte.
// Numbers are truncated toward zero like as<int>(); numeric strings are accepted as well.
static bool set_curve_byte(JsonVariant jsonValue, unsigned char *cmd, byte index, char *log_msg, PGM_P key) {
  if (jsonValue.isNull()) {
    return true;
  }

  long value = 0;
  bool valid;
  if (jsonValue.is<double>()) {
    double number = jsonValue.as<double>();
    valid = (number > -128) && (number < 128);
    if (valid) {
      value = (long)number;
    }
  } else {
    valid = parse_int_arg(jsonValue.as<const char *>(), -127, 127, &value);
  }

  if (!valid) {
    char key_ram[32] = { 0 };
    strncpy_P(key_ram, key, sizeof(key_ram) - 1);
    snprintf_P(log_msg, 256, PSTR("Invalid SetCurves value for %s: expected integer -127 to 127"), key_ram);
    return false;
  }

  cmd[index] = value + 128;
  return true;
}

unsigned int set_curves(char *msg, unsigned char *cmd, char *log_msg) {
  if (msg == NULL) {
    snprintf_P(log_msg, 256, PSTR("SetCurves JSON decode failed!"));
    return 0;
  }

  JsonDocument jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, msg);
  if (error) {
    snprintf_P(log_msg, 256, PSTR("SetCurves JSON decode failed!"));
    return 0;
  }

  memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
  bool valid = set_curve_byte(jsonDoc["zone1"]["heat"]["target"]["high"], cmd, 75, log_msg, PSTR("zone1.heat.target.high"))
            && set_curve_byte(jsonDoc["zone1"]["heat"]["target"]["low"], cmd, 76, log_msg, PSTR("zone1.heat.target.low"))
            && set_curve_byte(jsonDoc["zone1"]["heat"]["outside"]["low"], cmd, 77, log_msg, PSTR("zone1.heat.outside.low"))
            && set_curve_byte(jsonDoc["zone1"]["heat"]["outside"]["high"], cmd, 78, log_msg, PSTR("zone1.heat.outside.high"))
            && set_curve_byte(jsonDoc["zone2"]["heat"]["target"]["high"], cmd, 79, log_msg, PSTR("zone2.heat.target.high"))
            && set_curve_byte(jsonDoc["zone2"]["heat"]["target"]["low"], cmd, 80, log_msg, PSTR("zone2.heat.target.low"))
            && set_curve_byte(jsonDoc["zone2"]["heat"]["outside"]["low"], cmd, 81, log_msg, PSTR("zone2.heat.outside.low"))
            && set_curve_byte(jsonDoc["zone2"]["heat"]["outside"]["high"], cmd, 82, log_msg, PSTR("zone2.heat.outside.high"))
            && set_curve_byte(jsonDoc["zone1"]["cool"]["target"]["high"], cmd, 86, log_msg, PSTR("zone1.cool.target.high"))
            && set_curve_byte(jsonDoc["zone1"]["cool"]["target"]["low"], cmd, 87, log_msg, PSTR("zone1.cool.target.low"))
            && set_curve_byte(jsonDoc["zone1"]["cool"]["outside"]["low"], cmd, 88, log_msg, PSTR("zone1.cool.outside.low"))
            && set_curve_byte(jsonDoc["zone1"]["cool"]["outside"]["high"], cmd, 89, log_msg, PSTR("zone1.cool.outside.high"))
            && set_curve_byte(jsonDoc["zone2"]["cool"]["target"]["high"], cmd, 90, log_msg, PSTR("zone2.cool.target.high"))
            && set_curve_byte(jsonDoc["zone2"]["cool"]["target"]["low"], cmd, 91, log_msg, PSTR("zone2.cool.target.low"))
            && set_curve_byte(jsonDoc["zone2"]["cool"]["outside"]["low"], cmd, 92, log_msg, PSTR("zone2.cool.outside.low"))
            && set_curve_byte(jsonDoc["zone2"]["cool"]["outside"]["high"], cmd, 93, log_msg, PSTR("zone2.cool.outside.high"));
  if (!valid) {
    return 0;
  }

  snprintf_P(log_msg, 256, PSTR("SetCurves JSON received ok"));
  return sizeof(panasonicSendQuery);
}

unsigned int set_zones(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 2, &value, log_msg, PSTR("SetZones"))) {
    return 0;
  }

  byte set_mode;
  switch (value) {
    case 0: set_mode = 64; break;
    case 1: set_mode = 128; break;
    case 2: set_mode = 192; break;
    default: set_mode = 0; break;
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set zones active state to %d"), set_mode);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[6] = set_mode;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_floor_heat_delta(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetFloorHeatDelta"))) {
    return 0;
  }

  byte request_temp = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set floor heat delta to %d"), request_temp - 128 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[84] = request_temp;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_floor_cool_delta(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetFloorCoolDelta"))) {
    return 0;
  }

  byte request_temp = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set floor cool delta to %d"), request_temp - 128 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[94] = request_temp;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_dhw_heat_delta(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetDHWHeatDelta"))) {
    return 0;
  }

  byte request_temp = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set DHW heat delta to %d"), request_temp - 128 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[99] = request_temp;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_reset(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 1, &value, log_msg, PSTR("SetReset"))) {
    return 0;
  }

  byte resetRequest = 0;
  if ( value == 1 ) {
    resetRequest = 1;
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set reset to %d"), resetRequest);
    memcpy(log_msg, tmp, sizeof(tmp));
  }


  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[8] = resetRequest;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_heater_delay_time(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 254, &value, log_msg, PSTR("SetHeaterDelayTime"))) {
    return 0;
  }

  byte byteValue = value + 1;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set heater delay time to %d"), byteValue - 1 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[104] = byteValue;
  }

  return sizeof(panasonicSendQuery);
}
unsigned int set_heater_start_delta(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetHeaterStartDelta"))) {
    return 0;
  }

  byte byteValue = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set heater start delta to %d"), byteValue - 128 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[105] = byteValue;
  }

  return sizeof(panasonicSendQuery);
}
unsigned int set_heater_stop_delta(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetHeaterStopDelta"))) {
    return 0;
  }

  byte byteValue = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set heater stop delta to %d"), byteValue - 128 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[106] = byteValue;
  }

  return sizeof(panasonicSendQuery);
}
unsigned int set_main_schedule(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 1, &value, log_msg, PSTR("SetMainSchedule"))) {
    return 0;
  }

  byte byteValue = 64; //hex 0x40

  if ( value == 1 ) {
    byteValue = 128; //hex 0x80
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set main schedule to %d"), (byteValue / 64) - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[5] = byteValue;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_alt_external_sensor(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 1, &value, log_msg, PSTR("SetAltExternalSensor"))) {
    return 0;
  }

  byte set_alt = 16;
  if ( value == 1 ) {
    set_alt = 32;
  }

    {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set alternative external sensor to %d"), ((set_alt / 16) - 1) );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[20] = set_alt;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_external_pad_heater(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 2, &value, log_msg, PSTR("SetExternalPadHeater"))) {
    return 0;
  }

  byte set_pad = 16;
  if ( value == 1 ) {
    set_pad = 32;
  }
  if ( value == 2 ) {
    set_pad = 48;
  }
    {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set external pad heater to %d"), ((set_pad / 16) - 1) );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[25] = set_pad;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_buffer_delta(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetBufferDelta"))) {
    return 0;
  }

  byte request_temp = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set buffer tank delta to %d"), request_temp - 128 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[59] = request_temp;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_buffer(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 1, &value, log_msg, PSTR("SetBuffer"))) {
    return 0;
  }

  byte set_buffer = 4;
  if ( value == 1 ) {
    set_buffer = 8;
  }

    {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set buffer enabled to %d"), ((set_buffer / 4) - 1) );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[24] = set_buffer;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_heatingoffoutdoortemp(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetHeatingOffOutdoorTemp"))) {
    return 0;
  }

  byte request_temp = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set heating off outdoor temp %d"), request_temp - 128 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[83] = request_temp;
  }

  return sizeof(panasonicSendQuery);
  
}

unsigned int set_external_control(char *msg, unsigned char *cmd, char *log_msg){
  long requested;
  if (!parse_command_arg(msg, 0, 1, &requested, log_msg, PSTR("SetExternalControl"))) {
    return 0;
  }

  const byte off_state=1;
  const byte address=23;
  byte value = off_state;
  if ( requested == 1 ) {
    value = off_state * 2;
  }
    {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set external control enabled to %d"), ((value / off_state) - 1) );
    memcpy(log_msg, tmp, sizeof(tmp));
  }
  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[address] = value;
  }
  return sizeof(panasonicSendQuery);
}

unsigned int set_external_heat_cool_control(char *msg, unsigned char *cmd, char *log_msg){
  long requested;
  if (!parse_command_arg(msg, 0, 1, &requested, log_msg, PSTR("SetExternalHeatCoolControl"))) {
    return 0;
  }

  const byte off_state=4;
  const byte address=23;
  byte value = off_state;
  if ( requested == 1 ) {
    value = off_state * 2;
  }
    {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set external cool/heat control enabled to %d"), ((value / off_state) - 1) );
    memcpy(log_msg, tmp, sizeof(tmp));
  }
  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[address] = value;
  }
  return sizeof(panasonicSendQuery);
}

unsigned int set_external_error(char *msg, unsigned char *cmd, char *log_msg){
  long requested;
  if (!parse_command_arg(msg, 0, 1, &requested, log_msg, PSTR("SetExternalError"))) {
    return 0;
  }

  const byte off_state=16;
  const byte address=23;
  byte value = off_state;
  if ( requested == 1 ) {
    value = off_state * 2;
  }
    {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set external error signal enabled to %d"), ((value / off_state) - 1) );
    memcpy(log_msg, tmp, sizeof(tmp));
  }
  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[address] = value;
  }
  return sizeof(panasonicSendQuery);
}

unsigned int set_heatingcontrol(char *msg, unsigned char *cmd, char *log_msg) {
  long requested;
  if (!parse_command_arg(msg, 0, 1, &requested, log_msg, PSTR("SetHeatingControl"))) {
    return 0;
  }

  const byte address=30;
  byte value = 0b01;

  if ( requested == 1 ) {
    value = 0b10;
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set heating control %d"), value - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[address] = value << 2;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_smart_dhw(char *msg, unsigned char *cmd, char *log_msg) {
  long requested;
  if (!parse_command_arg(msg, 0, 1, &requested, log_msg, PSTR("SetSmartDHW"))) {
    return 0;
  }

  const byte address=24;
  byte value = 0b01;

  if ( requested == 1 ) {
    value = 0b10;
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set smart dhw %d"), value - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[address] = value << 6;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_quiet_mode_priority(char *msg, unsigned char *cmd, char *log_msg) {
  long requested;
  if (!parse_command_arg(msg, 0, 1, &requested, log_msg, PSTR("SetQuietModePriority"))) {
    return 0;
  }

  const byte address=11;
  byte value = 0b01;

  if ( requested == 1 ) {
    value = 0b10;
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set quiet mode priority %d"), value - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[address] = value << 4;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_pump_flowrate_mode(char *msg, unsigned char *cmd, char *log_msg) {
  long requested;
  if (!parse_command_arg(msg, 0, 1, &requested, log_msg, PSTR("SetPumpFlowrateMode"))) {
    return 0;
  }

  const byte address=29;
  byte value = 0b01;

  if ( requested == 1 ) {
    value = 0b10;
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set pump flowrate mode %d"), value - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[address] = value << 4;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_dhw_sensor_selection(char *msg, unsigned char *cmd, char *log_msg) {
  long requested;
  if (!parse_command_arg(msg, 0, 1, &requested, log_msg, PSTR("SetDHWSensorSelection"))) {
    return 0;
  }

  const byte address = 11;
  byte value = 0b01;

  if ( requested == 1 ) {
    value = 0b10;
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set dhw sensor selection %d"), value - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[address] = value;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_dhw_heater_state(char *msg, unsigned char *cmd, char *log_msg) {
  long requested;
  if (!parse_command_arg(msg, 0, 1, &requested, log_msg, PSTR("SetDHWHeaterState"))) {
    return 0;
  }

  const byte address = 9;
  byte value = 0b01;

  if ( requested == 1 ) {
    value = 0b10;
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set dhw heater state %d"), value - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[address] = value << 2;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_room_heater_state(char *msg, unsigned char *cmd, char *log_msg) {
  long requested;
  if (!parse_command_arg(msg, 0, 1, &requested, log_msg, PSTR("SetRoomHeaterState"))) {
    return 0;
  }

  const byte address = 9;
  byte value = 0b01;

  if ( requested == 1 ) {
    value = 0b10;
  }

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set room heater state %d"), value - 1);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[address] = value;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_heater_on_outdoor_temp(char *msg, unsigned char *cmd, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, -127, 127, &value, log_msg, PSTR("SetHeaterOnOutdoorTemp"))) {
    return 0;
  }

  byte byteValue = value + 128;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set heater on outdoor temp to %d"), byteValue - 128 );
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[85] = byteValue;
  }

  return sizeof(panasonicSendQuery);
}

unsigned int set_external_compressor_control(char *msg, unsigned char *cmd, char *log_msg){
  long requested;
  if (!parse_command_arg(msg, 0, 1, &requested, log_msg, PSTR("SetExternalCompressorControl"))) {
    return 0;
  }

  const byte off_state=64;
  const byte address=23;
  byte value = off_state;
  if ( requested == 1 ) {
    value = off_state * 2;
  }
    {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set external compressor control enabled to %d"), ((value / off_state) - 1) );
    memcpy(log_msg, tmp, sizeof(tmp));
  }
  {
    memcpy_P(cmd, panasonicSendQuery, sizeof(panasonicSendQuery));
    cmd[address] = value;
  }
  return sizeof(panasonicSendQuery);
}

//start of optional pcb commands
unsigned int set_byte_6(int val, int base, int bit, char *log_msg, const char *func) {
  unsigned char hex = (optionalPCBQuery[6] & ~(base << bit)) | ((val & base) << bit);

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set optional pcb '%s' to state %d (result byte 6: %02x)"), func, val, hex);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    optionalPCBQuery[6] = hex;
  }

  return sizeof(optionalPCBQuery);
}

unsigned int set_byte_9(char *msg, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 255, &value, log_msg, PSTR("SetOptPCBByte9"))) {
    return 0;
  }

  byte set_pcb_value = value;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set optional pcb '%s' to %02x"), __FUNCTION__, set_pcb_value);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    optionalPCBQuery[9] = set_pcb_value;
  }
  return sizeof(optionalPCBQuery);
}

unsigned int set_heat_cool_mode(char *msg, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 1, &value, log_msg, PSTR("SetHeatCoolMode"))) {
    return 0;
  }

  int set_pcb_value = (value == 1);

  return set_byte_6(set_pcb_value, 0b1, 7, log_msg, __FUNCTION__);
}

unsigned int set_compressor_state(char *msg, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 1, &value, log_msg, PSTR("SetCompressorState"))) {
    return 0;
  }

  int set_pcb_value = (value == 1);

  return set_byte_6(set_pcb_value, 0b1, 6, log_msg, __FUNCTION__);
}

unsigned int set_smart_grid_mode(char *msg, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 3, &value, log_msg, PSTR("SetSmartGridMode"))) {
    return 0;
  }

  return set_byte_6(value, 0b11, 4, log_msg, __FUNCTION__);
}

unsigned int set_external_thermostat_1_state(char *msg, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 3, &value, log_msg, PSTR("SetExternalThermostat1State"))) {
    return 0;
  }

  return set_byte_6(value, 0b11, 2, log_msg, __FUNCTION__);
}

unsigned int set_external_thermostat_2_state(char *msg, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 3, &value, log_msg, PSTR("SetExternalThermostat2State"))) {
    return 0;
  }

  return set_byte_6(value, 0b11, 0, log_msg, __FUNCTION__);
}

unsigned int set_demand_control(char *msg, char *log_msg) {
  long value;
  if (!parse_command_arg(msg, 0, 255, &value, log_msg, PSTR("SetDemandControl"))) {
    return 0;
  }

  byte set_pcb_value = value;

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set optional pcb '%s' to %02x"), __FUNCTION__, set_pcb_value);
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    optionalPCBQuery[14] = set_pcb_value;
  }

  return sizeof(optionalPCBQuery);
}

unsigned int set_xxx_temp(char *msg, char *log_msg, int byte, const char *func) {
  String set_pcb_string(msg);

  float temp = set_pcb_string.toFloat();

  {
    char tmp[256] = { 0 };
    snprintf_P(tmp, 255, PSTR("set optional pcb '%s' to temp %.2f (%02x)"), func, temp, temp2hex(temp));
    memcpy(log_msg, tmp, sizeof(tmp));
  }

  {
    optionalPCBQuery[byte] = temp2hex(temp);
  }

  return sizeof(optionalPCBQuery);
}

unsigned int set_pool_temp(char *msg, char *log_msg) {
  return set_xxx_temp(msg, log_msg, 7, __FUNCTION__);
}

unsigned int set_buffer_temp(char *msg, char *log_msg) {
  return set_xxx_temp(msg, log_msg, 8, __FUNCTION__);
}

unsigned int set_z1_room_temp(char *msg, char *log_msg) {
  return set_xxx_temp(msg, log_msg, 10, __FUNCTION__);
}

unsigned int set_z1_water_temp(char *msg, char *log_msg) {
  return set_xxx_temp(msg, log_msg, 16, __FUNCTION__);
}

unsigned int set_z2_room_temp(char *msg, char *log_msg) {
  return set_xxx_temp(msg, log_msg, 11, __FUNCTION__);
}

unsigned int set_z2_water_temp(char *msg, char *log_msg) {
  return set_xxx_temp(msg, log_msg, 15, __FUNCTION__);
}

unsigned int set_solar_temp(char *msg, char *log_msg) {
  return set_xxx_temp(msg, log_msg, 13, __FUNCTION__);
}




void send_heatpump_command(char* topic, char *msg, bool (*send_command)(byte*, int), void (*log_message)(char*), bool optionalPCB) {
  unsigned char cmd[256] = { 0 };
  char log_msg[256] = { 0 };
  unsigned int len = 0;

  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    cmdStruct tmp;
    memcpy_P(&tmp, &commands[i], sizeof(tmp));
    if (strcmp(topic, tmp.name) == 0) {
      len = tmp.func(msg, cmd, log_msg);
      log_message(log_msg);
      if (len > 0) send_command(cmd, len);
    }
  }

  if (optionalPCB) {
    //run for optional pcb commands
    for (unsigned int i = 0; i < sizeof(optionalCommands) / sizeof(optionalCommands[0]); i++) {
      optCmdStruct tmp;
      memcpy_P(&tmp, &optionalCommands[i], sizeof(tmp));
      if (strcmp(topic, tmp.name) == 0) {
        len = tmp.func(msg, log_msg);
        log_message(log_msg);
#ifdef ESP32        
       xQueueOverwrite(pcbQueue, optionalPCBQuery);
#endif          
      }
    }
  }

}


bool saveOptionalPCB(byte* command, int length) {
  if (LittleFS.begin()) {
    File pcbfile = LittleFS.open("/optionalpcb.raw", "w");
    if (pcbfile) {
      pcbfile.write(command, length);
      pcbfile.close();
      return true;
    }

  }
  return false;
}
bool loadOptionalPCB(byte* command, int length) {
  if (LittleFS.begin()) {
    if (LittleFS.exists("/optionalpcb.raw")) {
      File pcbfile = LittleFS.open("/optionalpcb.raw", "r");
      if (pcbfile) {
        pcbfile.read(command, length);
        pcbfile.close();
        return true;
      }
    }
  }
  return false;
}
