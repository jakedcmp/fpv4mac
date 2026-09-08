# Licensing boundaries

The original `fpv4mac` capture, replay, diagnostics, scripts, and documentation are licensed
under the repository's MIT License.

`gpl/wfb-bridge/` is GPL-3.0-only because it links the upstream WFB-NG receiver. Its source files
carry SPDX identifiers, and its full license is included in `gpl/wfb-bridge/COPYING`. The build
script also fetches the pinned WFB-NG source under the same license. The resulting `fpv4mac-wfb`
executable is distributed under GPL-3.0.

OpenIPC `devourer`, built as the separate `rxdemo` radio process, remains GPL-2.0 under its
upstream license. No upstream dependency is relicensed by this repository.
