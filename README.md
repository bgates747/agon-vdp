# agon-vdp

Part of the official Agon Platform organisation firmware for all Agon computers.

This firmware is intended for use on any Agon Light compatible computer.

The Agon Platform firmware is a fork from the original [official Quark firmware](https://github.com/breakintoprogram/agon-vdp) for the Agon Light.  It contains many extensions and bug fixes.  Software written to run on Quark firmware should be fully compatible with Agon Platform releases.

### What is the Agon

Agon is a modern, fully open-source, 8-bit microcomputer and microcontroller in one small, low-cost board. As a computer, it is a standalone device that requires no host PC: it puts out its own video (VGA), audio (2 identical mono channels), accepts a PS/2 keyboard and has its own mass-storage in the form of a µSD card.  The [Agon Console8](https://www.heber.co.uk/agon-console8) adds to this a second PS/2 port for attaching a keyboard, and two Atari-style joystick ports.

https://www.thebyteattic.com/p/agon.html

### What is the VDP

The VDP is a serial graphics terminal that takes a BBC Micro-style text output stream as input. The output is via the VGA connector on the Agon.

It will process any valid BBC BASIC VDU commands (starting with a character between 0 and 31).

For example:

`VDU 25, mode, x; y;` is equivalent to the BBC BASIC statement `PLOT mode, x, y`

The VDP also handles the interfaces to the PS/2 keyboard and mouse, and the audio output.

### Documentation

The AGON documentation can now be found on the [Community Documentation](https://agonplatform.github.io/agon-docs/) site.  This site provides extensive documentation about the Agon platform firmware, covering all Quark, Agon Console8, and Agon Platform firmware releases.

### Building

This project makes use of [PlatformIO](https://platformio.org) to build the firmware.  Using the PlatformIO IDE with Visual Studio Code is recommended, but it is also possible to use the PlatformIO CLI.

Previously, it was also possible to use the Arduino IDE to build the firmware, but this is no longer supported.  This is because this project makes use of an updated version of vdp-gl, which is not directly able to be used with the Arduino IDE.  (It is technically still possible to use the Arduino IDE, but it is not recommended, as you would need to manually download the applicable vdp-gl version.)

### Pingo variant

The `pingo-codex` branch is the hardware- and emulator-qualified Pingo
2.15.0 Alpha 1 variant based on Agon VDP 2.15 and TurboVega's final Pingo
port. See [docs/tv-port.md](docs/tv-port.md) for provenance, build/flash
commands, the coordinate contract, fixtures, and remaining work. The optional
compile-time renderer-attribution build and its hardware capture workflow are
documented in
[docs/pingo-render-diagnostics.md](docs/pingo-render-diagnostics.md).

The authoritative Pingo assembly applications, hardware test fixtures, source
assets, benchmark harnesses, and project-local emulator profiles live in the
separate `~/Agon/mystuff/pingoasm` repository. They are not maintained in this
firmware repository or in `~/Agon/pingoasm`.
