# Receiver contract

This document defines the intended boundary before the full radio receiver is implemented.

## Configuration input

Required:

- USB receiver identity or automatic selection
- Wi-Fi channel (initial RunCam target: channel 161)
- channel width
- WFB-NG ground-station key path
- RTP destination host and UDP port

Planned CLI shape:

```text
fpv4mac receive \
  --device 0bda:8812 \
  --channel 161 \
  --width 20 \
  --key /path/to/gs.key \
  --output rtp://127.0.0.1:5600
```

## Media output

- Transport: UDP
- Payload: the reconstructed RTP packets produced by the air unit
- Codec: H.265 or H.264; no transcoding in `fpv4mac`
- Destination: explicit IPv4/IPv6 host and port
- Backpressure: bounded buffering with packet-drop accounting

The receiver will not invent RTP timestamps or alter RTP sequence numbers. Downstream consumers
can use an SDP file or explicit caps to identify the codec and 90 kHz RTP clock.

## Health output

Human-readable status is the default. A stable JSON-lines mode will expose at least:

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

Field names will be versioned before the first stable release.

## Exit behavior

- Invalid configuration fails before the adapter is initialized.
- Missing or unreadable keys fail closed.
- Adapter removal changes health state immediately and triggers bounded reconnection attempts.
- Shutdown releases claimed USB interfaces and closes output sockets.
