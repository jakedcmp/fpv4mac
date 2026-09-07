# Contributing

Contributions and hardware test reports are welcome.

## Development setup

```bash
brew install cmake pkgconf libusb
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Please keep hardware-specific behavior behind explicit interfaces, avoid logging encryption-key
material, and include tests for protocol parsing and configuration changes.

Bug reports involving USB hardware should include macOS version, Mac architecture, USB vendor and
product IDs, adapter model, connection topology, and `fpv4mac doctor --json` output. Do not attach
private `gs.key` files.
