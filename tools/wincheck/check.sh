#!/usr/bin/env bash
# Compile-checks one plugin folder (Crossfire, "RELight Spell Addon", Illuminated) for Windows: every src/*.cpp through
# its PCH.h, with CommonLib's SE+AE settings. `--link` also builds CommonLib and spdlog once (cached) and links, then
# lists every game-side or plugin function that is used but defined nowhere (the Windows and C runtime ones are
# expected: only the real Visual C++ link has those). Run setup.sh first.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="${WINCHECK:-$HOME/.cache/wincheck}"
plugin="$(cd "$1" && pwd)"
link="${2:-}"
sdk="$root/sdk/c/Include/10.0.22000.0"
out="$root/obj/$(basename "$plugin" | tr ' ' '_')"
mkdir -p "$out"
flags=(--target=x86_64-pc-windows-msvc -fms-compatibility -fms-extensions -fms-compatibility-version=19.43 -std=c++23
	-fno-delayed-template-parsing -nostdlibinc -isystem "$root/STL/stl/inc" -isystem "$root/vcinc" -isystem "$sdk/ucrt"
	-isystem "$sdk/shared" -isystem "$sdk/um" -isystem "$sdk/winrt" -D_MT -DNDEBUG -DUNICODE -D_UNICODE -D_WIN64 -D_AMD64_
	-D_CRT_USE_BUILTIN_OFFSETOF -D_CRT_SECURE_NO_WARNINGS -Wno-unused-command-line-argument
	-I"$root/commonlib/include" -isystem "$root/spdlog/include" -isystem "$root/DirectXMath/Inc" -isystem "$root/DirectXTK/Inc"
	-DSPDLOG_USE_STD_FORMAT -DSPDLOG_WCHAR_TO_UTF8_SUPPORT -DSPDLOG_COMPILED_LIB -DENABLE_SKYRIM_SE=1 -DENABLE_SKYRIM_AE=1
	-DSKSE_SUPPORT_PATCH_SAFETY=1)
cxx="${CXX:-clang++}"
name="$(basename "$plugin" | tr -d ' ')"
cat > "$out/plugin-decl.cpp" <<CPP
#include <SKSE/SKSE.h>
SKSEPluginInfo(.Version = { 1, 0, 0, 0 }, .Name = "$name", .Author = "check", .SupportEmail = "",
	.StructCompatibility = SKSE::StructCompatibility::Independent, .RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary)
CPP
echo "== PCH"
# CommonLib's own warnings (there are many) stay in PCH.log
if ! "$cxx" "${flags[@]}" -I"$plugin/src" -x c++-header "$plugin/src/PCH.h" -o "$out/PCH.pch" 2> "$out/PCH.log"; then
	grep -E "error:" "$out/PCH.log" | head -20
	exit 1
fi
status=0
for f in "$plugin"/src/*.cpp "$out/plugin-decl.cpp"; do
	n="$(basename "$f" .cpp)"
	if ! "$cxx" "${flags[@]}" -I"$plugin/src" -include-pch "$out/PCH.pch" -Wall -Wextra -Wno-invalid-offsetof -c "$f" -o "$out/$n.o" 2> "$out/$n.log"; then
		status=1
	fi
	# only what is in the plugin's own files; CommonLib's and the menu framework header's warnings are theirs
	mine="$(grep -E "(error|warning):" "$out/$n.log" | grep -F "$plugin/src/" | grep -v "SKSEMenuFramework.h" || true)"
	count=$(printf "%s" "$mine" | grep -c . || true)
	echo "== $n: $count error(s)/warning(s) in the plugin's files"
	[ -n "$mine" ] && printf "%s\n" "$mine" | head -40
	grep -q "error:" "$out/$n.log" && { grep -E "error:" "$out/$n.log" | head -10; status=1; }
done
if [ "$link" = "--link" ]; then
	lib="$root/obj/lib"
	mkdir -p "$lib/cl" "$lib/sp"
	if [ ! -f "$lib/commonlib.lib" ]; then
		echo "== building CommonLib and spdlog once (a few minutes)"
		"$cxx" "${flags[@]}" -w -x c++-header "$root/commonlib/include/SKSE/Impl/PCH.h" -o "$lib/cl/PCH.pch"
		# one compile per file, all cores at once; the flags go through a file so xargs need not quote them
		printf "%s\n" "${flags[@]}" -I"$root/commonlib/src" -I"$root/minhook/src/hde" -include-pch "$lib/cl/PCH.pch" -w > "$lib/flags.rsp"
		find "$root/commonlib/src" -name "*.cpp" -print0 | xargs -0 -P"$(nproc)" -I{} sh -c \
			'o="$1/cl/$(printf %s "$2" | md5sum | cut -c1-16).o"; "$3" @"$1/flags.rsp" -c "$2" -o "$o" 2>/dev/null || echo "commonlib: $2 did not compile"' \
			sh "$lib" {} "$cxx"
		for f in "$root"/spdlog/src/*.cpp; do "$cxx" "${flags[@]}" -w -c "$f" -o "$lib/sp/$(basename "$f" .cpp).o"; done
		llvm-lib /OUT:"$lib/commonlib.lib" "$lib"/cl/*.o
		llvm-lib /OUT:"$lib/spdlog.lib" "$lib"/sp/*.o
	fi
	echo "== link"
	lld-link /DLL /NOENTRY /NODEFAULTLIB /FORCE:UNRESOLVED /OUT:"$out/check.dll" "$out"/*.o "$lib/commonlib.lib" "$lib/spdlog.lib" > "$out/link.log" 2>&1 || true
	# a qualified C++ name outside std is the game's, CommonLib's or the plugin's, and must be defined somewhere; C
	# functions, Windows imports and std:: are what only the real Visual C++ link provides
	missing="$(grep "undefined symbol" "$out/link.log" | sed 's/.*undefined symbol: //' | sort -u | python3 -c '
import sys
for line in sys.stdin:
    s = line.strip()
    head = s.split("(")[0].split()
    name = head[-1] if head else s
    if "::" in name and not name.startswith(("std::", "type_info::", "`")):
        print(s)
' || true)"
	if [ -n "$missing" ]; then
		echo "used but defined nowhere:"
		printf "%s\n" "$missing"
		status=1
	else
		echo "every game-side and plugin function used is defined ($(grep -c "undefined symbol" "$out/link.log" || true) Windows/C runtime names left for the real link)"
	fi
fi
exit $status
