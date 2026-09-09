# Receiver contract

This document defines the implemented receiver boundary for version 0.3.0.

## Configuration input

Required:

- USB receiver identity or automatic selection
- Wi-Fi channel (initial RunCam target: channel 161)
- channel width
- WFB-NG ground-station key path
- RTP destination host and UDP port

Optional:

- WFB link ID and radio port; both are discovered together by default
- SDP destination; defaults beside the required raw capture in `scripts/receive.sh`
- health reporting interval; defaults to 1000 ms and `0` disables periodic events

Current orchestration command:

```text
scripts/receive.sh \
  --channel 161 \
  --width 20 \
  --key /path/to/gs.key \
  --capture captures/bench.fpv4cap \
  --host 127.0.0.1 \
  --port 5600
```

Automatic link selection extracts candidate channel identities only from WFB-formatted source
addresses. It keeps a bounded candidate set and selects a link only after WFB-NG successfully
authenticates a session with the configured key. `--link-id ID --radio-port PORT` selects an
explicit identity instead; decimal and `0x` hexadecimal link IDs are accepted.

## Media output

- Transport: UDP
- Payload: the reconstructed RTP packets produced by the air unit
- Codec: H.265 or H.264; no transcoding in `fpv4mac`
- Destination: explicit IPv4 host and port
- Backpressure: bounded capture/WFB buffering; nonblocking UDP send failures are counted

The receiver does not invent RTP timestamps or alter RTP sequence numbers. It parses RTP headers
only to count packets/gaps and look for strong H.264/H.265 NAL-unit evidence. On detection it
writes an SDP file with the observed dynamic payload type and a 90 kHz video clock. Unknown or
non-RTP WFB payloads are still forwarded without SDP generation.

## Health output

Capture, discovery, media, periodic health, and completion events are JSON Lines on standard
error. A representative health event is:

```json
{
  "event": "wfb.health",
  "state": "receiving",
  "uptime_ms": 12000,
  "link_selected": true,
  "channel_id": 1963316736,
  "link_id": 7669206,
  "radio_port": 0,
  "wifi_frames": 36823,
  "authenticated_sessions": 1,
  "decrypt_errors": 0,
  "fec_recovered": 7,
  "packet_loss": 0,
  "bad_packets": 0,
  "udp_packets": 24271,
  "udp_bytes": 25774002,
  "udp_send_errors": 0,
  "rtp_packets": 24271,
  "rtp_bytes": 25774002,
  "rtp_sequence_gaps": 0,
  "codec": "H265",
  "payload_type": 97,
  "last_wifi_age_ms": 0,
  "last_udp_age_ms": 0
}
```

States progress from `waiting_radio` to `waiting_session`, then `authenticated` and `receiving`.
After media has flowed, more than three seconds without an outgoing packet reports `stalled`.
Counters are cumulative for the process. `wfb.complete` adds filtered/malformed frame and bounded
candidate-drop counts. Field names remain pre-stable until the first stable release.

## Exit behavior

- Invalid configuration fails before the adapter is initialized.
- Missing or unreadable keys fail closed.
- Adapter reconnection is planned but not yet implemented.
- SIGINT/SIGTERM stop the bridge cleanly; pipeline shutdown releases the USB interface and closes
  output sockets.
