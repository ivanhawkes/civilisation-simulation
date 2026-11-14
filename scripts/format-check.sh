#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
clang-format --version | grep -q 'version 21\.' || {
  echo 'The format gate requires clang-format 21 (use the devenv shell).' >&2
  exit 1
}
mapfile -d '' files < <(find source -type f \( -name '*.h' -o -name '*.cpp' \) -print0)
clang-format --dry-run -Werror "${files[@]}"
