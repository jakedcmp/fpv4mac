# fpv4cap format version 1

`.fpv4cap` is a compact little-endian record stream designed for repeatable radio debugging. It
stores the WFB-bearing 802.11 body emitted by `devourer`, including its trailing four-byte FCS,
plus enough RF metadata to replay the receiver boundary.

## File header

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | Magic `FPV4CAP\0` |
| 8 | 2 | Format version (`1`) |
| 10 | 2 | Header size (`16`) |
| 12 | 4 | Reserved (`0`) |

## Repeated frame record

| Size | Field |
|---:|---|
| 8 | Monotonic capture time, nanoseconds from capture start |
| 4 | Receiver TSF low word |
| 2 | 802.11 sequence |
| 2 | Hardware rate index |
| 1 | Bandwidth |
| 1 | Flags: CRC, ICV, LDPC, short-GI in bits 0–3 |
| 2 each | RSSI, SNR, and EVM for paths A/B |
| 6 | Source MAC address |
| 4 | Body length |
| variable | Body bytes, including FCS |

Readers reject unknown versions, truncated records, and bodies larger than 16 KiB. Version 1
files can be streamed because records require no footer or index.
