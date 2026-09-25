#!/usr/bin/env bash
# Builds and runs Crossfire's core tests natively (Linux or WSL), with the address and undefined-behaviour sanitizers,
# under both g++ and clang++ where present, and an optimised build for the timing; then tests/fuzz_touch.cpp checks
# the collision search against a reference that shares none of its code. `--fuzz N` also fuzzes the settings
# parser for N seconds (needs clang's libFuzzer).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/crossfire-tests"
mkdir -p "$out"
src=("$here/test_core.cpp" "$here/../src/Core.cpp")
warn=(-DCROSSFIRE_DATA="\"$here/../data\"" -std=c++23 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -I"$here/../src")
status=0
for cxx in g++ clang++; do
	command -v "$cxx" >/dev/null || continue
	echo "== $cxx, sanitizers"
	"$cxx" "${warn[@]}" -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all "${src[@]}" -o "$out/test-$cxx-san"
	"$out/test-$cxx-san" || status=1
	echo "== $cxx, optimised"
	"$cxx" "${warn[@]}" -O2 -DNDEBUG "${src[@]}" -o "$out/test-$cxx-opt"
	"$out/test-$cxx-opt" || status=1
	echo "== $cxx, collision search against an independent reference"
	"$cxx" "${warn[@]}" -g -O2 -fsanitize=address,undefined -fno-sanitize-recover=all "$here/fuzz_touch.cpp" "$here/../src/Core.cpp" -o "$out/touch-$cxx"
	"$out/touch-$cxx" 300000 || status=1
done
if [[ "${1:-}" == "--fuzz" ]]; then
	secs="${2:-60}"
	echo "== libFuzzer, ${secs}s"
	clang++ -std=c++23 -g -O1 -fsanitize=fuzzer,address,undefined -I"$here/../src" "$here/fuzz_ini.cpp" "$here/../src/Core.cpp" -o "$out/fuzz-ini"
	mkdir -p "$out/corpus"
	cp "$here/../data/SKSE/Plugins/"*.ini "$out/corpus/" 2>/dev/null || true
	"$out/fuzz-ini" -max_total_time="$secs" -max_len=4096 "$out/corpus" 2>&1 | tail -3 || status=1
fi
exit $status
