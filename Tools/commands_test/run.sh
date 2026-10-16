#!/bin/bash
# Build and run the host-side tests for every command setter in
# HeishaMon/commands.cpp. Needs g++ and, unless ARDUINOJSON_DIR points at a
# directory containing ArduinoJson.h, curl to fetch the pinned ArduinoJson.
set -eu

ARDUINOJSON_VERSION=7.4.3
ARDUINOJSON_SHA256=ab5fbb8268b846b5f4bc5a5fee11bb2c96f7b8b846f5bef6540afb6a9cc76a5b

DIR="$(cd "$(dirname "$0")" && pwd)"
SRC="$DIR/../../HeishaMon"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

if [ -z "${ARDUINOJSON_DIR:-}" ]; then
  curl -fsSL -o "$OUT/ArduinoJson.h" \
    "https://github.com/bblanchon/ArduinoJson/releases/download/v$ARDUINOJSON_VERSION/ArduinoJson-v$ARDUINOJSON_VERSION.h"
  echo "$ARDUINOJSON_SHA256  $OUT/ArduinoJson.h" | sha256sum -c --quiet
  ARDUINOJSON_DIR="$OUT"
fi

g++ -std=gnu++17 -Wall -Werror -o "$OUT/commands_test" \
  -include "$DIR/shim/host.h" -I "$DIR/shim" -I "$SRC" -I "$ARDUINOJSON_DIR" \
  "$DIR/commands_test.cpp" "$SRC/commands.cpp" "$SRC/src/common/parsenum.cpp"
"$OUT/commands_test"
