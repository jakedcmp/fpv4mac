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

This validated the Mac-to-USB-to-radio receive path. Deterministic fixtures separately validate
WFB-NG session authentication, decryption, dropped-fragment FEC recovery, and UDP forwarding.

## 2026-09-08 — powered WiFiLink2 and live video

Using the same Mac and receiver, a powered WiFiLink2-G produced an authenticated WFB-NG session
on channel 161. The receiver reconstructed and forwarded H.265 RTP payload type 97. A live sample
was identified as 1280×720 at 120 fps at roughly 4.1 Mbps, and was decoded on macOS. The raw-radio
capture contained 36,823 frames over 48.9 seconds with no capture-queue drops, allowing the radio
and WFB path to be reproduced later without hardware.

## 2026-09-09 — automatic discovery and media description

Replaying that saved capture through version 0.3.0:

1. Derived link ID `0x7505d6` and radio port `0` from candidate WFB source addresses.
2. Selected the link only after its session authenticated with the matching ground key.
3. Recovered 24,271 RTP datagrams, including seven FEC recoveries.
4. Detected H.265 payload type 97 and generated a valid local SDP file.
5. Emitted a final `receiving` health snapshot and complete counters.

These figures describe one bench capture rather than a general performance guarantee. The raw
capture and ground key remain private and are not repository fixtures.

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
