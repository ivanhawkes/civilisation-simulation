#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
werror=OFF
asan=false
release=true
log=''
tidy=false
# Keep local builds modest; CI can override these limits.
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
export CTEST_PARALLEL_LEVEL="${CTEST_PARALLEL_LEVEL:-2}"
while (($#)); do
  case "$1" in
    --werror) werror=ON ;;
    --asan) asan=true ;;
    --tidy) tidy=true ;;
    --skip-release) release=false ;;
    --log) shift; log="${1:?--log requires a path}" ;;
    *) echo "Unknown argument: $1" >&2; exit 2 ;;
  esac
  shift
done
if [[ -n "$log" ]]; then
  mkdir -p "$(dirname "$log")"
  exec > >(tee "$log") 2>&1
fi
./scripts/format-check.sh
build_and_test() {
  local preset="$1"
  cmake --preset "$preset" -DMANIFEST_WERROR="$werror"
  cmake --build --preset "$preset"
  ctest --preset "$preset"
}
build_and_test clang-debug
# Only Manifest translation units, using the SDK's compile commands and tidy rules.
if "$tidy"; then
  python3 scripts/tidy.py build/clang-debug
fi
if "$release"; then
  build_and_test clang-release
fi
if "$asan"; then
  build_and_test clang-asan
fi
