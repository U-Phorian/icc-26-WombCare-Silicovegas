#!/bin/sh
# Build and run the DSP host self-test.
#
# The real wombcare_dsp.c is copied next to the stub headers so that its
# own  #include "wombcare_buffer.h"  resolves to the stub rather than to
# the firmware header (which pulls in em_device.h and the whole SDK).
set -e

here=$(cd "$(dirname "$0")" && pwd)
root=$here/../..
out=$here/build

rm -rf "$out"
mkdir -p "$out"

cp "$root/wombcare_dsp.c" "$out/dsp_under_test.c"
cp "$root/wombcare_dsp.h" "$out/wombcare_dsp.h"

# The rolling beat trend is plain C with no SDK dependency, so the real
# file is compiled here too rather than stubbed.
cp "$root/wombcare_trend.c" "$out/wombcare_trend.c"
cp "$root/wombcare_trend.h" "$out/wombcare_trend.h"

cp -r "$here/stub/." "$out/"
cp "$here/main.c" "$out/main.c"

CC=${CC:-$(command -v cc || command -v gcc || command -v clang)}
if [ -z "$CC" ]; then
    echo "No host C compiler found. Set CC=/path/to/gcc and retry." >&2
    exit 1
fi

"$CC" -std=c11 -O2 -Wall -I"$out" -o "$out/dsp_selftest" \
      "$out/main.c" "$out/dsp_under_test.c" "$out/wombcare_trend.c" -lm

"$out/dsp_selftest"
