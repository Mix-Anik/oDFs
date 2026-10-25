#!/bin/bash
# Builds dfgolden with gcc, without cmake (testing/CMakeLists.txt is the reference; ci.sh uses it).
#   usage: build.sh <output exe>
# The float options matter: results are compared bit for bit, so no operation may be fused.
set -e
OUT=${1:-dfgolden.exe}
HERE="$(cd "$(dirname "$0")" && pwd)"
T="$(cd "$HERE/.." && pwd)"
E="$T/third_party/engine"
S="$T/third_party/engine_shim"
OBJ="$(mktemp -d)"
trap 'rm -rf "$OBJ"' EXIT

FLAGS="-O2 -Wall -Wno-unused-function -ffp-contract=off -fno-fast-math -I$S -I$E -DDEDICATED"
COLL="$E/cm_load.c $E/cm_test.c $E/cm_trace.c $E/cm_patch.c $E/cm_polylib.c $S/sv_world.c"

# The collision code again under other names (engine_shim/coll_names.h): compiled for x87 registers (32 bit x86 engines), and with
# floats instead of doubles (ioq3 based engines). host/coll.h picks per recording.
EXTRA=""
DEFS=""
if echo 'int main(void){return 0;}' | gcc -mfpmath=387 -x c - -o "$OBJ/probe" 2>/dev/null; then
	for f in $COLL; do
		gcc $FLAGS -mfpmath=387 -DCOLL_SUFFIX=_x87 -include "$S/coll_names.h" -c "$f" -o "$OBJ/x87_$(basename "$f" .c).o"
		EXTRA="$EXTRA $OBJ/x87_$(basename "$f" .c).o"
	done
	DEFS="-DDFG_HAVE_X87"
fi
F="$T/third_party/engine_f32"
for f in $E/cm_load.c $E/cm_test.c $F/cm_trace.c $F/cm_patch.c $F/cm_polylib.c $S/sv_world.c; do
	gcc $FLAGS -DCOLL_SUFFIX=_f32 -include "$S/coll_names.h" -c "$f" -o "$OBJ/f32_$(basename "$f" .c).o"
	EXTRA="$EXTRA $OBJ/f32_$(basename "$f" .c).o"
done

gcc $FLAGS $DEFS "$HERE"/{main,golden,goldmake,goldrun,goldmain,demo,steps,gamerun,gamesteps,psdiff,cfgcompare}.c \
	"$T"/host/{pmhost,gamehost,pmvm}.c "$T/common/enginestubs.c" \
	"$T/third_party/miniz/miniz.c" "$S/sv_world.c" \
	"$E"/{msg,huffman,huffman_static,q_shared,q_math,cm_load,cm_test,cm_trace,cm_patch,cm_polylib,md4}.c \
	$EXTRA -o "$OUT" -lm
