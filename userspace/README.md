# Native Pingo 2 VDP module

This adapter builds the accepted Pingo 2 engine, bounded startup probe and
PINGO-023 scene bridge inside the owned stock VDP firmware, using official Fab
1.2.5's exact loader glue and userspace compatibility sources. It implements
the accepted Pingo-only 0x49 commands 0–41 and 51–55, not Wolf or the deferred
lighting/transaction features. A successful native test does not qualify
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
accepted. PINGO-023 owns the new scene bridge and its separate Author-run
cube/Lara and stock-MOS gate. Keep this tranche uncommitted until the Author
accepts the behavior and approves publication. No profile is created or
launched by this build target.

## Scene bridge qualification

`video/pingo2_commands.c` is the plain-C scene/resource owner, adapted from
fsim's accepted PINGO-022 owner without changing the frozen engine closure.
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
