# First powered WiFiLink test

## Before applying power

1. Attach both air-unit antennas before powering it.
2. Confirm the BEC output voltage and polarity with a multimeter.
3. Keep the propellers removed and use the smoke stopper for the first energization.
4. Obtain the matching WFB-NG `gs.key`; do not commit or paste it into logs.
5. Confirm channel, width, link ID, and radio port from the air-unit configuration.

## Start the Mac receiver

```bash
scripts/receive.sh \
  --key /absolute/path/to/gs.key \
  --capture captures/wifilink-first-power.fpv4cap \
  --channel 161 \
  --width 20 \
  --link-id 0 \
  --radio-port 0 \
  --host 127.0.0.1 \
  --port 5600
```

Start a downstream player or packet listener on UDP 5600, then power the air unit. Stop the
receiver with Ctrl-C before unplugging the USB adapter.

## What success looks like

1. `devourer` reaches `Listening air...`.
2. Capture completion reports radio frames with zero queue drops.
3. `fpv4mac inspect` identifies WFB session and data packets.
4. The WFB helper accepts a session rather than reporting key/channel mismatch.
5. UDP/RTP packets arrive at port 5600.
6. FFmpeg or GStreamer identifies H.264/H.265 and renders video.

If steps 3–5 fail, preserve the `.fpv4cap` file. It allows inspection and replay without keeping
the air unit powered.
