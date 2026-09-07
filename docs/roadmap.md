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
- [ ] Integrate the backend behind the native receiver-session API
- [ ] Tune channel and width from explicit configuration
- [ ] Count raw frames and publish RSSI/link observations
- [ ] Handle adapter removal and reconnection

## Milestone 2 — WFB-NG recovery

- [ ] Parse WFB-NG session and data frames
- [ ] Load `gs.key` without logging key material
- [ ] Authenticate/decrypt packets
- [ ] Recover FEC blocks
- [ ] Add deterministic capture/replay fixtures

## Milestone 3 — RTP forwarding

- [ ] Forward reconstructed RTP to configurable UDP destination
- [ ] Detect H.264/H.265 payloads
- [ ] Publish packet-loss and last-packet-age metrics
- [ ] Validate with FFmpeg, GStreamer, and a computer-vision consumer

## Milestone 4 — distribution

- [ ] Universal or Apple Silicon release packaging
- [ ] Code signing and notarization
- [ ] Homebrew formula
- [ ] Hardware compatibility matrix
- [ ] OpenIPC contribution and maintainer feedback
