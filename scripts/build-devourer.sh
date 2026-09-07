#!/usr/bin/env bash
set -euo pipefail

readonly DEVOURER_URL="https://github.com/OpenIPC/devourer.git"
readonly DEVOURER_REVISION="ebe9f9517fb9fab402808c27b0ab6640874207e6"
readonly PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly SOURCE_DIR="${PROJECT_ROOT}/.deps/devourer"
readonly BUILD_DIR="${SOURCE_DIR}/build-fpv4mac"

command -v git >/dev/null || { echo "git is required" >&2; exit 1; }
command -v cmake >/dev/null || { echo "cmake is required (brew install cmake)" >&2; exit 1; }
pkg-config --exists libusb-1.0 || {
  echo "libusb is required (brew install pkgconf libusb)" >&2
  exit 1
}

if [[ ! -d "${SOURCE_DIR}/.git" ]]; then
  mkdir -p "$(dirname "${SOURCE_DIR}")"
  git clone "${DEVOURER_URL}" "${SOURCE_DIR}"
fi

git -C "${SOURCE_DIR}" fetch --quiet origin "${DEVOURER_REVISION}"
git -C "${SOURCE_DIR}" checkout --quiet --detach "${DEVOURER_REVISION}"

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DDEVOURER_JAGUAR1=ON \
  -DDEVOURER_8814=OFF \
  -DDEVOURER_JAGUAR2_8821C=OFF \
  -DDEVOURER_JAGUAR2_8822B=OFF \
  -DDEVOURER_JAGUAR3_8822C=OFF \
  -DDEVOURER_JAGUAR3_8822E=OFF \
  -DDEVOURER_8733B=OFF \
  -DDEVOURER_KESTREL_8852B=OFF \
  -DDEVOURER_KESTREL_8852C=OFF \
  -DDEVOURER_PCIE=OFF
cmake --build "${BUILD_DIR}" --target rxdemo -j 2

echo
echo "Built ${BUILD_DIR}/rxdemo"
echo "Pinned devourer revision: ${DEVOURER_REVISION}"
