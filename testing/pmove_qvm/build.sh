#!/bin/bash
# Builds pmove.qvm with the project's q3lcc / q3asm.
#   usage: build.sh <output dir>
# q3lcc is a native Windows tool: it needs Windows style paths (D:/...) and a TEMP directory it can
# write to. The sources are listed with relative includes because q3cpp ignores -I for quoted includes.
set -e
OUT=$1
[ -n "$OUT" ] || { echo "usage: $0 <output dir>"; exit 1; }

HERE="$(cd "$(dirname "$0")" && { pwd -W 2>/dev/null || pwd; })"
ROOT="$(cd "$HERE/../.." && { pwd -W 2>/dev/null || pwd; })"

mkdir -p "$OUT"
cd "$OUT"

# q3cpp only understands include paths relative to the working directory
# (the output directory has to be on the same drive as the repository)
REL="$(realpath --relative-to="$(pwd -W 2>/dev/null || pwd)" "$ROOT")"
INC="-I$REL/code/game -I$REL/code/qcommon"

SRC="$ROOT/testing/pmove_qvm/pm_main.c \
$ROOT/code/game/bg_misc.c $ROOT/code/game/bg_pmove.c $ROOT/code/game/bg_slidemove.c $ROOT/code/game/bg_lib.c \
$ROOT/code/qcommon/q_shared.c $ROOT/code/qcommon/q_math.c"

"$ROOT/bin/q3lcc" -S -DQ3_VM $INC $SRC

# pm_main must come first: the interpreter enters at instruction 0, which is vmMain
"$ROOT/bin/q3asm" -vq3 -o "$OUT/pmove.qvm" pm_main.asm bg_misc.asm bg_pmove.asm bg_slidemove.asm bg_lib.asm \
	q_shared.asm q_math.asm "$ROOT/code/game/g_syscalls.asm"
