#!/bin/sh
# Build and run the trend/feature host self-test.
#
# Same staging trick as tools/dsp_selftest: the real wombcare_dsp.c is
# copied next to the stub CMSIS headers so its includes resolve without
# dragging in the SDK. The test file #includes that copy so it can reach
# the static compute_features().
set -e

here=$(cd "$(dirname "$0")" && pwd)
root=$here/../..
stub=$here/../dsp_selftest/stub
out=$here/build

rm -rf "$out"
mkdir -p "$out"

cp "$root/wombcare_dsp.c"   "$out/dsp_under_test.c"
cp "$root/wombcare_dsp.h"   "$out/wombcare_dsp.h"
cp "$root/wombcare_trend.c" "$out/wombcare_trend.c"
cp "$root/wombcare_trend.h" "$out/wombcare_trend.h"
cp -r "$stub/." "$out/"
cp "$here/main.c" "$out/main.c"

CC=${CC:-$(command -v cc || command -v gcc || command -v clang)}
if [ -z "$CC" ]; then
    echo "No host C compiler found. Set CC=/path/to/gcc and retry." >&2
    exit 1
fi

"$CC" -std=c11 -O2 -Wall -I"$out" -o "$out/trend_selftest" \
      "$out/main.c" "$out/wombcare_trend.c" -lm

"$out/trend_selftest"
