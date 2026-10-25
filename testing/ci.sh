#!/bin/bash
# Physics regression test: builds the game QVM, the offsets helper and dfgolden, then replays every golden
# file in testing/golden through the game. Needs nothing of the game installation or an engine.
#   usage: testing/ci.sh [build dir]        (default: <repo>/build-ci)
# Exit status: 0 all golden files reproduce what they did when they were made, 1 a step regressed.
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:-$ROOT/build-ci}"
mkdir -p "$OUT"

GEN=()
case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*)
		# the Visual Studio generator is the Windows default; the tools want a Makefiles / Ninja build with gcc
		if cmake --help | grep -q "MinGW Makefiles"; then
			GEN=(-G "MinGW Makefiles")
		fi ;;
esac

T0=$SECONDS
echo "== game QVM"
cmake -S "$ROOT" -B "$OUT/qvm" "${GEN[@]}" -DBUILD_QVM=ON $CMAKE_EXTRA_ARGS > "$OUT/qvm-configure.log"
cmake --build "$OUT/qvm" --target qagame

echo "== offsets helper QVM"
bash "$HERE/offsets_qvm/build.sh" "$OUT/offsets"

echo "== dfgolden"
cmake -S "$HERE" -B "$OUT/dfgolden" "${GEN[@]}" -DCMAKE_BUILD_TYPE=Release ${DFG_ZLIB:+-DDFG_ZLIB=$DFG_ZLIB} $CMAKE_EXTRA_ARGS > "$OUT/dfgolden-configure.log"
cmake --build "$OUT/dfgolden"

EXE="$OUT/dfgolden/dfgolden"
[ -x "$EXE" ] || EXE="$EXE.exe"

echo "== build took $(( SECONDS - T0 )) s${DFG_ZLIB:+ (system zlib)}"
T0=$SECONDS
echo "== golden files"
"$EXE" test --game "$OUT/qvm/vm/qagame.qvm" --offsets "$OUT/offsets/offsets.qvm" "$HERE/golden"
echo "== replay took $(( SECONDS - T0 )) s${DFG_ZLIB:+ (system zlib)}"
