# Native Pingo 2 VDP module

This adapter builds the accepted Pingo 2 engine and its bounded startup probe
inside the owned stock VDP firmware, using official Fab 1.2.5's exact loader
glue and userspace compatibility sources. It does not implement the simulator
protocol, Pingo 1 control bridge, or Wolf. A successful smoke test qualifies
the module boundary, not emulator visuals or physical hardware.

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

The consuming flight-simulator tasks PINGO-012 and PINGO-013 own automated
module qualification and the separate human emulator gate, respectively.
Keep this adapter, probe-output change and all coupled profile work
uncommitted until the Author accepts the running emulator. No profile is
created or launched by this build target.
