# Hardware validation

## 2026-09-07 — Apple Silicon and RTL8812AU

Validated configuration:

- Apple Silicon Mac running macOS 26.6
- RunCam WiFiLink2-G USB receiver
- USB ID `0bda:8812`
- USB 2.0 high-speed transport (480 Mbps)
- OpenIPC `devourer` revision `ebe9f9517fb9fab402808c27b0ab6640874207e6`
- Receive channel 161

Results:

1. `fpv4mac doctor` enumerated the receiver, opened it, claimed interface 0, and released it.
2. The pinned `devourer` Jaguar1 backend built successfully with Apple clang.
3. `rxdemo` identified the RTL8812A 2T2R chip, loaded its firmware, switched to 5 GHz,
   tuned channel 161, and entered monitor mode.
4. The receiver delivered an ambient 802.11 frame before the WiFiLink2 air unit was powered.
5. Ctrl-C shut the radio down and released the USB interface cleanly.

This validates the Mac-to-USB-to-radio receive path. Deterministic fixtures separately validate
WFB-NG session authentication, decryption, dropped-fragment FEC recovery, and UDP forwarding. It
does **not** yet validate the WiFiLink2 transmitter or playable live RTP video.

## Repeat the raw-radio test

```bash
scripts/build-devourer.sh
scripts/radio-smoke-test.sh 161
```

The smoke test is receive-only. Nearby 802.11 traffic can increment the raw packet count, so a
packet alone does not prove that it came from the air unit. Once the air unit is powered, use its
configured channel and correlate packet activity with transmitter power cycling.

## macOS process-access note

The process must have direct access to macOS USB services. A sandboxed terminal or automation
host can sometimes list the adapter with `ioreg` while libusb still reports an access or resource
error. Run the receiver from a normal Terminal session when diagnosing that discrepancy. This is
process sandboxing, not evidence of bad receiver hardware.
