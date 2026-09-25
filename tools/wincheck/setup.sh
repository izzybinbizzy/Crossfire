#!/usr/bin/env bash
# Sets up a Linux machine to compile-check the SKSE plugins against CommonLibSSE NG for Windows (the MSVC ABI), with
# clang 18+ and no Visual Studio: Microsoft's STL (open source) and the Windows SDK headers (the NuGet package), the
# libraries CommonLib includes, and the stubs in vcinc/ standing in for the few Visual C++ runtime headers that are
# in neither. Everything lands in $WINCHECK (default ~/.cache/wincheck). Run once; check.sh does the compiling.
#
# This finds what the compiler finds: errors, warnings, CommonLib's layout static_asserts, and (with --link) any game
# or plugin function declared but never defined. It is not the shipping build - build.bat on Windows is.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="${WINCHECK:-$HOME/.cache/wincheck}"
mkdir -p "$root"
cd "$root"
commonlib_commit=d13d10a0ccb4945870eb841bf1ad8a6cf5ed84dd

[ -d commonlib ] || { git clone -q https://github.com/alandtse/CommonLibVR commonlib && git -C commonlib checkout -q "$commonlib_commit"; }
[ -d spdlog ] || git clone -q --depth 1 --branch v1.16.0 https://github.com/gabime/spdlog spdlog
[ -d DirectXMath ] || git clone -q --depth 1 https://github.com/microsoft/DirectXMath DirectXMath
[ -d DirectXTK ] || git clone -q --depth 1 --branch feb2024 https://github.com/microsoft/DirectXTK DirectXTK
[ -d minhook ] || git clone -q --depth 1 --branch v1.3.4 https://github.com/TsudaKageyu/minhook minhook
if [ ! -d STL ]; then
	# vs-2022-17.13 is the newest STL that still accepts clang 18
	git clone -q --filter=blob:none --no-checkout https://github.com/microsoft/STL STL
	git -C STL checkout -q vs-2022-17.13 -- stl/inc
fi
if [ ! -d sdk ]; then
	curl -sSL -o sdk.nupkg https://api.nuget.org/v3-flatcontainer/microsoft.windows.sdk.cpp/10.0.22000.196/microsoft.windows.sdk.cpp.10.0.22000.196.nupkg
	mkdir -p sdk && (cd sdk && unzip -q ../sdk.nupkg 'c/Include/*') && rm sdk.nupkg
fi
rm -rf vcinc && cp -r "$here/vcinc" vcinc

# Windows looks headers up without regard to case; Linux does not. Give every header the spelling it is included by.
python3 - "$root" <<'PY'
import os, re, sys
root = sys.argv[1]
sdk = os.path.join(root, 'sdk/c/Include/10.0.22000.0')
dirs = [os.path.join(sdk, d) for d in ('ucrt', 'shared', 'um', 'winrt')]
index = {}
for d in dirs:
    for dp, _, fs in os.walk(d):
        for f in fs:
            rel = os.path.relpath(os.path.join(dp, f), d)
            index.setdefault(rel.lower(), []).append((d, rel))
names = set()
inc = re.compile(rb'#\s*include\s*[<"]([^>"]+)[>"]')
for d in dirs + [os.path.join(root, p) for p in ('commonlib/include', 'DirectXTK/Inc', 'DirectXMath/Inc', 'STL/stl/inc')]:
    for dp, _, fs in os.walk(d):
        for f in fs:
            try:
                data = open(os.path.join(dp, f), 'rb').read()
            except OSError:
                continue
            names.update(m.decode(errors='ignore').replace('\\', '/') for m in inc.findall(data))
made = 0
for n in names:
    for d, rel in index.get(n.lower(), []):
        target = os.path.join(d, n)
        if not os.path.exists(target):
            os.makedirs(os.path.dirname(target), exist_ok=True)
            os.symlink(os.path.join(d, rel), target)
            made += 1
print(f'{made} header spellings added')
PY
echo "ready in $root"
