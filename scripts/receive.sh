#!/usr/bin/env bash
set -euo pipefail

readonly PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly RXDEMO="${PROJECT_ROOT}/.deps/devourer/build-fpv4mac/rxdemo"
readonly FPV4MAC="${PROJECT_ROOT}/build/fpv4mac"
readonly WFB_BRIDGE="${PROJECT_ROOT}/build-wfb-bridge/fpv4mac-wfb"

channel=161
width=20
link_id=0
radio_port=0
host="127.0.0.1"
port=5600
key=""
capture=""

usage() {
  echo "Usage: scripts/receive.sh --key gs.key --capture flight.fpv4cap [options]"
  echo "Options: --channel 161 --width 20 --link-id 0 --radio-port 0"
  echo "         --host 127.0.0.1 --port 5600"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --channel|--width|--link-id|--radio-port|--host|--port|--key|--capture)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 2; }
      case "$1" in
        --channel) channel="$2" ;;
        --width) width="$2" ;;
        --link-id) link_id="$2" ;;
        --radio-port) radio_port="$2" ;;
        --host) host="$2" ;;
        --port) port="$2" ;;
        --key) key="$2" ;;
        --capture) capture="$2" ;;
      esac
      shift 2
      ;;
    --help|-h) usage; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

[[ -x "${RXDEMO}" ]] || { echo "Run scripts/build-devourer.sh first." >&2; exit 1; }
[[ -x "${FPV4MAC}" ]] || { echo "Build fpv4mac first: cmake -S . -B build && cmake --build build" >&2; exit 1; }
[[ -x "${WFB_BRIDGE}" ]] || { echo "Run scripts/build-wfb-bridge.sh first." >&2; exit 1; }
[[ -n "${key}" && -r "${key}" ]] || { echo "A readable --key gs.key is required." >&2; exit 1; }
[[ -n "${capture}" ]] || { echo "--capture FILE is required so live RF is never lost." >&2; exit 1; }
mkdir -p "$(dirname "${capture}")"

echo "Receiving channel ${channel}/${width} MHz to udp://${host}:${port}"
echo "Saving raw radio frames to ${capture}"
echo "Press Ctrl-C to stop."

DEVOURER_CHANNEL="${channel}" \
DEVOURER_BW="${width}" \
DEVOURER_STREAM_OUT=1 \
DEVOURER_RX_AGG_SA=any \
DEVOURER_LOG_LEVEL=info \
"${RXDEMO}" |
  "${FPV4MAC}" capture --output "${capture}" --passthrough |
  "${WFB_BRIDGE}" --key "${key}" --host "${host}" --port "${port}" \
    --channel "${channel}" --link-id "${link_id}" --radio-port "${radio_port}"
