# Corrected Pingo 2 source closure

This directory contains the plain-C `math/` and `render/` closure from Pingo
commit `05761ad232c80445a97550b25e92530882d02c04` (tree
`283801a4505608bb32fe231f9115d9dd6f20bfee`) with the accepted Agon Flight
Simulator corrected source overlays substituted directly. This closure now
includes PINGO-015 R01 compile-time BGRA8888/RGBA2222 pixels and C13
camera-invariant world lighting. `esp32dev-pingo2-probe` explicitly selects
BGRA8888; `esp32dev-pingo2-probe-rgba2222` selects direct FabGL `AABBGGRR`.

The retained license is the donor repository's `LICENSE`. This import contains
no Wolf renderer, Wolf protocol, registry, or combined-VDP customization.

## PINGO-017 O01 migration checkpoint

The corrected closure includes frame-owned camera inversion and per-object
view/model composition and light normalization. It matches the effective
FSIM source at commit `8f886459a7f275e49187c395ec493c9db32565a1`.
See [the agent handoff](AGENT-HANDOFF.md) for provenance, validation, remaining
acceptance gates, and the boundary between probe builds and runtime support.
