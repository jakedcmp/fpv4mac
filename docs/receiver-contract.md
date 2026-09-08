# Receiver contract

This document defines the implemented receiver boundary and the remaining health/recovery work.

## Configuration input

Required:

- USB receiver identity or automatic selection
- Wi-Fi channel (initial RunCam target: channel 161)
- channel width
- WFB-NG ground-station key path
- RTP destination host and UDP port

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

## Media output

- Transport: UDP
- Payload: the reconstructed RTP packets produced by the air unit
- Codec: H.265 or H.264; no transcoding in `fpv4mac`
- Destination: explicit IPv4 host and port
- Backpressure: bounded buffering with packet-drop accounting

The receiver will not invent RTP timestamps or alter RTP sequence numbers. Downstream consumers
can use an SDP file or explicit caps to identify the codec and 90 kHz RTP clock.

## Health output

Capture and WFB completion events are JSON. A future periodic JSON-lines mode will expose at least:

```json
{
  "state": "receiving",
  "adapter": "0bda:8812",
  "channel": 161,
  "wifi_frames": 0,
  "wfb_packets": 0,
  "fec_recovered": 0,
  "decrypt_errors": 0,
  "rtp_packets": 0,
  "rtp_sequence_gaps": 0,
  "last_packet_age_ms": null
}
```

The current end-of-session WFB event includes accepted/filtered/malformed frames, decrypt errors,
FEC recoveries, packet loss, bad packets, and forwarded UDP packet/byte counts. Field names will
be versioned before the first stable release.

## Exit behavior

- Invalid configuration fails before the adapter is initialized.
- Missing or unreadable keys fail closed.
- Adapter reconnection is planned but not yet implemented.
- Shutdown releases claimed USB interfaces and closes output sockets.
