# fpv4mac WFB bridge

This helper consumes `devourer` `rx.frame` JSON Lines, authenticates/decrypts WFB-NG packets,
performs the upstream Reed-Solomon FEC recovery, and sends the original payloads to UDP.

It links the upstream WFB-NG receiver and is therefore GPL-3.0-only. It runs as a separate
process from the MIT-licensed `fpv4mac` capture/replay executable. WFB-NG's source and license
are fetched at the pinned revision by `scripts/build-wfb-bridge.sh`.
