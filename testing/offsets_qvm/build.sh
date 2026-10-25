#!/bin/bash
# Builds offsets.qvm (see offsets_main.c) with the project's q3lcc / q3asm.
#   usage: build.sh <output dir>
set -e
OUT=$1
[ -n "$OUT" ] || { echo "usage: $0 <output dir>"; exit 1; }

HERE="$(cd "$(dirname "$0")" && { pwd -W 2>/dev/null || pwd; })"
ROOT="$(cd "$HERE/../.." && { pwd -W 2>/dev/null || pwd; })"

mkdir -p "$OUT"
cd "$OUT"

"$ROOT/bin/q3lcc" -S -DQ3_VM "$ROOT/testing/offsets_qvm/offsets_main.c"
"$ROOT/bin/q3asm" -vq3 -o "$OUT/offsets.qvm" offsets_main.asm "$ROOT/code/game/g_syscalls.asm"
