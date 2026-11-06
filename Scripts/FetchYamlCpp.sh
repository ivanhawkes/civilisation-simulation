#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
DEST="${ROOT_DIR}/Source/YamlCpp/Private/ThirdParty/yaml-cpp"
TAG="yaml-cpp-0.9.0"
REPO="https://github.com/jbeder/yaml-cpp.git"

if [[ -d "${DEST}/.git" ]]; then
    echo "yaml-cpp checkout already exists at ${DEST}"
    git -C "${DEST}" fetch --depth 1 origin "refs/tags/${TAG}:refs/tags/${TAG}"
    git -C "${DEST}" checkout --detach "${TAG}"
elif [[ -e "${DEST}" ]]; then
    echo "Error: ${DEST} exists but is not a Git checkout." >&2
    echo "Remove it or inspect it before running this script again." >&2
    exit 1
else
    mkdir -p "$(dirname -- "${DEST}")"
    git clone --depth 1 --branch "${TAG}" "${REPO}" "${DEST}"
fi

echo "yaml-cpp ${TAG} is ready at:"
echo "  ${DEST}"
