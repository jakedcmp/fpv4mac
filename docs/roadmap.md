# Roadmap

## Milestone 0 — macOS hardware readiness

- [x] Identify RTL8812AU by USB ID
- [x] Open, claim, and release the receiver through libusb
- [x] Human and JSON diagnostics
- [x] Apple Silicon CI foundation

## Milestone 1 — raw radio receive

- [x] Pin and reproducibly build OpenIPC `devourer`
- [x] Build and hardware-test only the RTL8812AU/Jaguar1 backend initially
- [x] Tune channel 161 and observe a raw frame on Apple Silicon
- [x] Consume the backend's stable JSONL receiver interface
- [x] Tune channel and width from explicit configuration
- [x] Preserve raw frames and RSSI/SNR/EVM observations in versioned captures
- [ ] Handle adapter removal and reconnection

## Milestone 2 — WFB-NG recovery

- [x] Parse WFB-NG session and data frames
- [x] Load `gs.key` without logging key material
- [x] Authenticate/decrypt packets through the pinned upstream receiver
- [x] Recover FEC blocks through the pinned upstream receiver
- [x] Add deterministic capture/replay and crypto/FEC fixtures

## Milestone 3 — RTP forwarding

- [x] Forward reconstructed RTP to configurable UDP destination
- [x] Discover link ID/radio port and require session authentication before selection
- [x] Detect H.264/H.265 payloads and generate SDP
- [x] Publish end-of-session WFB loss/FEC/decrypt and capture-drop metrics
- [x] Publish periodic live link-health events
- [x] Validate live H.265 RTP with FFmpeg on the reference hardware
- [ ] Validate with GStreamer and a computer-vision consumer

## Milestone 4 — distribution

- [ ] Universal or Apple Silicon release packaging
- [ ] Code signing and notarization
- [ ] Homebrew formula
- [x] Hardware compatibility matrix with explicit validation tiers
- [ ] OpenIPC contribution and maintainer feedback
