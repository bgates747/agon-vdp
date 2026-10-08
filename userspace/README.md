# Native Pingo 2 VDP module

This adapter builds the accepted Pingo 2 engine, bounded startup probe and
PINGO-027 transactional scene bridge inside the owned stock VDP firmware, using official Fab
1.2.5's exact loader glue and userspace compatibility sources. It implements
the accepted Pingo-only 0x49 commands 0–41, 43–48 and 50–55, not Wolf or
retired dithering. Native tooling revision 30 pins PINGO-030's numeric widths;
it is not a wire opcode or negotiation mechanism. Regenerate every client:
historical P023/P025 streams are incompatible. Lighting/solid-face behavior
remains the exact accepted P024 R02 engine.
A successful native test does not qualify
stock-MOS client handling, emulator visuals or physical hardware.

## Build and qualify

Resolve local checkout locations from the canonical development-environment
inventory. Supply the consuming project's virtualenv Python explicitly:

```sh
make -C userspace -j2 smoke FAB_ROOT=/path/to/official/fab-agon-emulator PYTHON=/path/to/agon-fsim/.venv/bin/python
```

The official checkout is read-only, pinned to Fab tag `1.2.5` commit
`c22876d1f7ad903537c308cf65afd6553b37cdb8` and `userspace-vdp-gl` commit
`cdd57fd4d709427b2e89738ae3b086720d8cf013`. GCC/G++, Make, Python, binutils and
the host C/C++ runtime are required. CRC 1.0.4 is read from the owned
PlatformIO dependency cache (`CRC_ROOT` may select another identical cache).
No upstream object/archive, firmware module or executable is reused.

The small Linux adapter supplies Arduino forward declarations, `std::max`
and a real glibc allocation-size diagnostic missing from the tagged shim.
Only the CRC translation units map Arduino `yield` to `std::this_thread::yield`.
Two YMODEM definitions now match their existing `size_t` declarations on LP64;
the already-disabled userspace hex-loader helper returns zero explicitly.
Neither change alters the ESP32 execution path or the accepted Pingo engine.

1. Compile C11 engine/probe and C++17 firmware/compatibility code separately,
   with `-O2 -g -fPIC`, native RGBA2222 pixels and 32-bit depth. Reject unresolved
   link symbols with `-Wl,-z,defs`.
2. Resolve all 15 Fab ABI symbols and the probe export; check `nm -D`/`ldd`.
3. In each of three fresh, 20-second-bounded processes, require the forced
   startup probe on stderr without verbose logging, rerun it twice, enter the
   VDP loop, and change the real stock UART/VDU framebuffer from solid black
   to solid colour. Check dimensions, refresh rate and an overrun guard;
   exercise audio sample retrieval and call shutdown.
4. Freeze the module and machine-local source/dependency/ABI results under
   ignored `video/build/userspace/pingo2/candidate/`. A different existing
   candidate is never silently replaced. This is a profile-integration input,
   not a hardware-qualified snapshot.

Expected startup record: status/render `0/0`, pixels/depth `342/342`, native
frame FNV `ebfd0b25`, depth FNV `0c76557b`. The probe's 32x24 render target is
private: it is not blitted onto the normal VDP screen. Stock scanout liveness
is tested separately, without pretending a simulator scene was rendered.

## Lifecycle and acceptance boundary

Fab retains its module handle for process lifetime. Its current compatibility
layer starts detached threads; `vdp_shutdown` requests termination but does
not join them. The smoke worker therefore calls shutdown and exits the
process without `dlclose` or global C++ teardown. Unload/restart safety is not
claimed or retrofitted here.

The consuming flight-simulator PINGO-012/013 boundary/profile gates are
accepted, as are PINGO-023's cube/Lara bridge and PINGO-025's bounded
lighting/terrain gates. On 2026-10-07 the Author confirmed the near-terrain
emulator view works and authorized publication. This is not full-corpus or
physical qualification. No profile is created or launched by this build target.

## Scene bridge qualification

`video/pingo2_commands.c` is the plain-C scene/resource owner, adapted from
fsim's accepted PINGO-026 transaction/PINGO-030 wire owner, preserving target
allocation and borrowed bitmap hooks. The engine is the exact 41-file R02
closure pinned by `smoke.py`.
`pingo2_control.h` keeps typed controls in a private checked-allocation list;
they are never mutable ordinary byte buffers. Canonical buffer mutations
destroy affected controls or reject typed sources. `pingo2_bridge.h` drains
known wire payloads, retains both stock Bitmap and BufferStream ownership,
publishes a successful frame before the optional ten-byte P3DR notification,
and never dereferences the control after sending that notification.

`BRIDGE_TEST=1` builds a separate `pingo2-test` module exposing a synchronous
real-VDU-stream seam. Add `BRIDGE_DIAGNOSTICS=1` for failure injection and
bounded hashes/counters in `pingo2-diagnostics-test`. These test exports are
absent from the production module. The fsim utility's `vdp_commands.py`
compares both variants with the independent decoded-input host oracle;
`vdp_uart.py` separately tests production UART publication and scanout without
launching Fab. `check_vdp_owner.py` runs the C11/C++14 owner clients under
ASan/UBSan with leak detection. See fsim's PINGO-023 record for identities and
the explicit remaining MOS/Author review boundary.

ESP32 `esp32dev-pingo2` enables the bridge; ordinary `esp32dev` excludes both
engine and bridge. The retained BGRA8888/native startup probe profiles also
exclude the command owner. New bridge allocations use checked PSRAM-aware
allocation; this does not retrofit or qualify every stock VDP STL allocator.

## P027 wire, transaction and diagnostic boundaries

1. IDs and indices stay u16. Position arrays use i24/32767; translations
   (14–17, 22–25, 34–37, 52) use i24 times 256/32767. Object/scene scales
   are u24/256. Command 53 far is u24 whole metres; 54 near is u24/256 plus
   unchanged u16 FOV/16384. Counts in 1–4, 40 and 50 are u24. Position/UV
   extents remain bounded to 65536 by u16 indices; triangle-entry counts may
   exceed 65535. No ambiguous old-width fallback exists.
2. Command 50 has a 16-byte payload: source/mesh u16 followed by four u24
   counts. The source must be one consolidated ordinary buffer, ID 1–65534,
   of exact packed length 9V+2I+4UV+2UI. Typed controls are never sources.
   A temporary retained BufferStream wrapper borrows its bytes without a
   second whole-source copy. Validate shared/private UVs and flat selectors,
   stage four owned arrays, prepare bounds, then atomically refresh every
   instance view. Rejection preserves the live mesh and all old output.
   Release the source wrapper after either result; its stock bytes are unchanged.
3. Mutation is rejected while the actual renderer is busy. Completion runs
   after publication and return to idle; stock packet callbacks may replace
   or destroy the control. No control access follows `send_packet`.
4. Diagnostic clocks are nanoseconds (ESP32 microsecond clock multiplied by
   1000, not nanosecond accuracy). Separate parser receive/execute/notice,
   owner upload/validation/stage/commit/notification, and frame prepare/render/
   output clocks; log I/O is outside measured rendering. Work counts are
   submitted instance vertices/triangles, not rasterized fragments. Owner
   requested/staged bytes exclude root, borrowed stock data and platform
   metadata; platform counters include bridge wrappers and diagnostic headers,
   not stock STL/PSRAM allocator overhead. Neither measures physical headroom.
   Diagnostics-off returns zero clocks/hashes/counters and identical pixels.
5. The consuming fsim `vdp_commands.py`, `check_review.py`, and leak-enabled
   `check_vdp_owner.py` (also `--no-diagnostics`) qualify the parser, source
   aliases, failure points, seeded lifecycles and generated application streams.
   P027 freezes a new build candidate; P028 owns deployment and actual MOS/
   keyboard/Author review. No active emulator or physical baseline is replaced.
