# First powered WiFiLink test

## Before applying power

1. Attach both air-unit antennas before powering it.
2. Confirm the BEC output voltage and polarity with a multimeter.
3. Keep the propellers removed and use the smoke stopper for the first energization.
4. Obtain the matching WFB-NG `gs.key`; do not commit or paste it into logs.
5. Confirm the configured channel and width. Link ID and radio port default to authenticated
   automatic discovery, but explicit values remain available for diagnosis.

## Start the Mac receiver

```bash
scripts/receive.sh \
  --key /absolute/path/to/gs.key \
  --capture captures/wifilink-first-power.fpv4cap \
  --channel 161 \
  --width 20 \
  --host 127.0.0.1 \
  --port 5600
```

The command writes an SDP file beside the raw capture after it recognizes H.264 or H.265. Start a
downstream player from that SDP, then power the air unit. Stop the receiver with Ctrl-C before
unplugging the USB adapter.

```bash
ffplay -protocol_whitelist file,udp,rtp captures/wifilink-first-power.sdp
```

## What success looks like

1. `devourer` reaches `Listening air...`.
2. Capture completion reports radio frames with zero queue drops.
3. `fpv4mac inspect` identifies WFB session and data packets.
4. `wfb.link_selected` appears only after a session authenticates with the supplied key.
5. `media.detected` reports H.264/H.265 and the SDP path.
6. `wfb.health` reaches `receiving`, and UDP/RTP packets arrive at port 5600.
7. FFmpeg or GStreamer opens the SDP and renders video.

If steps 3–5 fail, preserve the `.fpv4cap` file. It allows inspection and replay without keeping
the air unit powered.
