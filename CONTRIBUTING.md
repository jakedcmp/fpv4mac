# Contributing

Contributions and hardware test reports are welcome.

## Development setup

```bash
brew install cmake pkgconf libusb libsodium
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
scripts/build-wfb-bridge.sh
ctest --test-dir build-wfb-bridge --output-on-failure
```

Please keep hardware-specific behavior behind explicit interfaces, avoid logging encryption-key
material, and include tests for protocol parsing and configuration changes. Code linked with
WFB-NG belongs in the GPL-3.0 helper; keep the MIT/GPL process boundary described in
`docs/architecture.md` intact.

Bug reports involving USB hardware should include macOS version, Mac architecture, USB vendor and
product IDs, adapter model, connection topology, and `fpv4mac doctor --json` output. Do not attach
private `gs.key` files.
