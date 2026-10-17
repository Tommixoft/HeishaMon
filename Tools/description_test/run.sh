#!/bin/bash
# Build and run the host-side tests for the decoder description lookup.
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

g++ -Wall -Wextra -Werror -o "$OUT/description_test" \
  "$DIR/description_test.cpp" "$DIR/../../HeishaMon/src/common/description.cpp"
"$OUT/description_test"
