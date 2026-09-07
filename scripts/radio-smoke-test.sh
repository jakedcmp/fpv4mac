#!/usr/bin/env bash
set -euo pipefail

readonly PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly RXDEMO="${PROJECT_ROOT}/.deps/devourer/build-fpv4mac/rxdemo"
readonly CHANNEL="${1:-161}"

if [[ ! -x "${RXDEMO}" ]]; then
  echo "Receiver backend is not built. Run scripts/build-devourer.sh first." >&2
  exit 1
fi

if [[ ! "${CHANNEL}" =~ ^[0-9]+$ ]]; then
  echo "Channel must be an integer (for example, 161)." >&2
  exit 2
fi

echo "Starting receive-only raw radio test on channel ${CHANNEL}."
echo "Press Ctrl-C to stop. This validates RF frames, not decoded video."
DEVOURER_CHANNEL="${CHANNEL}" exec "${RXDEMO}"
