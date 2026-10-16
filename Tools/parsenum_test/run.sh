#!/bin/bash
# Build and run the host-side tests for the command argument parser.
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

g++ -Wall -Wextra -Werror -o "$OUT/parsenum_test" \
  "$DIR/parsenum_test.cpp" "$DIR/../../HeishaMon/src/common/parsenum.cpp"
"$OUT/parsenum_test"
