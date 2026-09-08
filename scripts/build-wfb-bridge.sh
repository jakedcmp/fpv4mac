#!/usr/bin/env bash
set -euo pipefail

readonly WFB_NG_URL="https://github.com/svpcom/wfb-ng.git"
readonly WFB_NG_REVISION="3504a3870189fdc4bb1f7e2a2f141a740973a185"
readonly PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly SOURCE_DIR="${PROJECT_ROOT}/.deps/wfb-ng"
readonly BUILD_DIR="${PROJECT_ROOT}/build-wfb-bridge"

command -v git >/dev/null || { echo "git is required" >&2; exit 1; }
command -v cmake >/dev/null || { echo "cmake is required (brew install cmake)" >&2; exit 1; }
pkg-config --exists libsodium || {
  echo "libsodium is required (brew install pkgconf libsodium)" >&2
  exit 1
}

if [[ ! -d "${SOURCE_DIR}/.git" ]]; then
  mkdir -p "$(dirname "${SOURCE_DIR}")"
  git clone "${WFB_NG_URL}" "${SOURCE_DIR}"
fi

git -C "${SOURCE_DIR}" fetch --quiet origin "${WFB_NG_REVISION}"
git -C "${SOURCE_DIR}" checkout --quiet --detach "${WFB_NG_REVISION}"

cmake -S "${PROJECT_ROOT}/gpl/wfb-bridge" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DWFB_NG_SOURCE_DIR="${SOURCE_DIR}"
cmake --build "${BUILD_DIR}" -j 2

echo
echo "Built ${BUILD_DIR}/fpv4mac-wfb"
echo "Pinned WFB-NG revision: ${WFB_NG_REVISION}"
