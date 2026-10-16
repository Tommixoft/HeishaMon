/* LittleFS stub: the filesystem never mounts, so saveOptionalPCB and
   loadOptionalPCB return false without touching the buffer. */
#ifndef _LITTLEFS_SHIM_H_
#define _LITTLEFS_SHIM_H_

#include <stddef.h>
#include <stdint.h>

class File {
 public:
  operator bool() const { return false; }
  size_t write(const uint8_t *, size_t) { return 0; }
  size_t read(uint8_t *, size_t) { return 0; }
  void close() {}
};

class LittleFSStub {
 public:
  bool begin() { return false; }
  bool exists(const char *) { return false; }
  File open(const char *, const char *) { return File(); }
};

static LittleFSStub LittleFS;

#endif
