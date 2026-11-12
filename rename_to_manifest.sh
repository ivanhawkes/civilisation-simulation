#!/usr/bin/env bash
set -euo pipefail

echo "1. Updating Git SSH remote..."
git remote set-url origin git@github.com:ivanhawkes/manifest.git

echo "2. Renaming Source/CivSimCore directory..."
if [ -d "Source/CivSimCore" ]; then
    git mv Source/CivSimCore Source/ManifestCore
fi

echo "3. Replacing identifiers across repository files..."
# Find all tracked text files (excluding the script itself and .git)
git ls-files | grep -v "rename_to_manifest.sh" | while IFS= read -r file; do
    if [ -f "$file" ]; then
        sed -i \
            -e 's/CivSimCoreTui/ManifestCoreTui/g' \
            -e 's/CivSimCore/ManifestCore/g' \
            -e 's/CivSimData/ManifestData/g' \
            -e 's/CivSim/Manifest/g' \
            -e 's/civilisation-simulation/manifest/g' \
            "$file"
    fi
done

echo "4. Formatting C++ source and test files..."
find Source Tests \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' \) -print0 | xargs -0 clang-format -i

echo "5. Removing old build directory and running clean CMake build + tests..."
rm -rf build
cmake -S . -B build -G Ninja \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

echo "Done! Review changes with 'git status' and 'git diff', then delete this script and commit."
