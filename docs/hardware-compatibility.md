# Hardware compatibility

`fpv4mac` is a macOS receiver for the OpenIPC/WFB-NG transport boundary, not a driver for one
camera brand. Hardware claims use three deliberately different labels:

- **Validated:** exercised end to end on a Mac, including authenticated WFB recovery and RTP.
- **Candidate:** expected to fit an already enabled protocol or chipset path, but not yet tested.
- **Upstream target:** supported by an upstream project but not enabled or validated here yet.

An adapter appearing in an upstream table is not enough to call it compatible with `fpv4mac`.
USB IDs, chipset revisions, WFB injection/monitor behavior, and macOS userspace access all matter.

## Ground-side USB receivers

| Tier | Chipset / USB ID | Current fpv4mac status | Notes |
| --- | --- | --- | --- |
| Validated | RTL8812AU `0bda:8812` | Built, recognized, and live-tested | Reference receiver bundled with WiFiLink2-G |
| Candidate | Other genuine RTL8812AU `0bda:8812` adapters | Same enabled Jaguar1 path; hardware report needed | Product names alone are insufficient because vendors change internals |
| Priority upstream target | RTL8812EU `0bda:a81a` | Not enabled in the pinned build | Both WFB-NG and devourer list this family; useful next validation target |
| Upstream target | RTL8814AU `0bda:8813` | Not enabled | Listed by devourer; not in WFB-NG's officially supported pair |
| Upstream target | RTL8822BU / common IDs such as `2357:012d` | Not enabled | Listed by devourer; WFB behavior and macOS path remain unvalidated here |
| Upstream target | RTL8822CU `0bda:c82c` and RTL8812CU `0bda:c812` | Not enabled | Listed by devourer; WFB behavior and macOS path remain unvalidated here |
| Upstream target | Newer RTL88x2/87xx USB families listed by devourer | Not enabled | Longer-term community targets, not purchasing recommendations |

The current build intentionally enables only devourer's Jaguar1 backend. WFB-NG's own
[Wi-Fi hardware guidance](https://github.com/svpcom/wfb-ng/wiki/WiFi-hardware) identifies
RTL8812AU and RTL8812EU as its officially supported chipsets. Devourer's broader
[macOS/Linux/Windows/Android table](https://github.com/OpenIPC/devourer/blob/master/README.md)
describes radio-driver reach, not an end-to-end fpv4mac guarantee.

## Air-side transmitters and cameras

| Tier | Hardware | Status |
| --- | --- | --- |
| Validated | RunCam WiFiLink2-G air unit and camera | H.265 RTP, authenticated WFB-NG, FEC recovery, and UDP forwarding validated |
| Candidate | OpenIPC air units using compatible WFB-NG session/FEC framing and RTP H.264/H.265 payloads | Protocol path should compose, but configuration and hardware must be tested |
| Outside the current boundary | Proprietary digital FPV links that do not expose compatible WFB-NG radio frames | Require a separate receiver backend or an ordinary network-video output |

Automatic discovery does not make an arbitrary transmitter compatible. It derives a candidate
link ID and radio port from the WFB source-address convention, then requires the configured
`gs.key` to authenticate a session before selecting it. The receiver forwards the recovered UDP
payload unchanged, so non-RTP payloads can pass through but do not receive video SDP metadata.

## What to include in a hardware report

Please include:

1. Mac model/architecture and macOS version.
2. Adapter product name, chipset if known, and exact USB vendor/product ID from
   `fpv4mac doctor --json` or System Information.
3. Hub/dongle and antenna arrangement.
4. OpenIPC/WFB-NG transmitter hardware and firmware version.
5. Channel, width, codec, and whether automatic link selection authenticated.
6. Sanitized `wfb.health` and `wfb.complete` events.

Never attach or paste `gs.key`, private captures, or other credentials into a public issue.
