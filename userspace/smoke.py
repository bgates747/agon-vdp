#!/usr/bin/env python3
"""Bounded native-module qualification; no emulator/profile or hardware access."""
import argparse
import ctypes as C
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import threading
import time

ROOT = Path(__file__).resolve().parent.parent
FAB_COMMIT = "c22876d1f7ad903537c308cf65afd6553b37cdb8"
GL_COMMIT = "cdd57fd4d709427b2e89738ae3b086720d8cf013"
ENGINE_SHA = "d4dfe19205d715cefebd1e41b9bf9905f35d9290c94883cabcbadb62c66c8c8a"
PROBE_RECORD = ("Pingo2 target probe: status=0 render=0 pixels=342 depth=342 "
                "frame=ebfd0b25 zeta=0c76557b")
U8P = C.POINTER(C.c_uint8)
ABI = {
    "vdp_setup": (None, []), "vdp_loop": (None, []),
    "vdp_shutdown": (None, []), "signal_vblank": (None, []),
    "copyVgaFramebuffer": (None, [C.POINTER(C.c_int), C.POINTER(C.c_int),
                                   C.c_void_p, C.POINTER(C.c_float)]),
    "set_startup_screen_mode": (None, [C.c_uint32]),
    "z80_uart0_is_cts": (C.c_bool, []),
    "z80_send_to_vdp": (None, [C.c_uint8]),
    "z80_recv_from_vdp": (C.c_bool, [U8P]),
    "sendVKeyEventToFabgl": (None, [C.c_uint32, C.c_uint8]),
    "sendPS2KbEventToFabgl": (None, [C.c_uint16, C.c_uint8]),
    "sendHostMouseEventToFabgl": (None, [U8P]),
    "setVdpDebugLogging": (None, [C.c_bool]),
    "getAudioSamples": (None, [U8P, C.c_uint32]),
    "dump_vdp_mem_stats": (None, []),
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def command(*args):
    return subprocess.check_output(args, text=True).strip()


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def closure(root, paths):
    digest = hashlib.sha256()
    for path in sorted(set(paths), key=lambda p: p.relative_to(root).as_posix()):
        digest.update(path.relative_to(root).as_posix().encode() + b"\0")
        digest.update(path.read_bytes() + b"\0")
    return digest.hexdigest()


def verify_inputs(fab, crc):
    gl = fab / "src/vdp/userspace-vdp-gl"
    require(command("git", "-C", str(fab), "rev-parse", "HEAD") == FAB_COMMIT,
            "Fab must be the qualified official 1.2.5 release")
    require(command("git", "-C", str(gl), "rev-parse", "HEAD") == GL_COMMIT,
            "userspace-vdp-gl identity differs")
    for repo, paths in [(fab, ["src/vdp/rust_glue.cpp", "src/vdp/vdp.h", "src/vdp_interface.rs"]),
                        (gl, [])]:
        require(not command("git", "-C", str(repo), "diff", "HEAD", "--", *paths),
                f"dirty selected upstream source: {repo}")
    require("version=1.0.4" in (crc / "library.properties").read_text(),
            "CRC 1.0.4 is required (same pinned dependency as ESP32)")
    engine = ROOT / "video/pingo2"
    files = [engine / "LICENSE"] + [p for d in ["math", "render"]
                                    for p in (engine / d).rglob("*") if p.is_file()]
    require(closure(engine, files) == ENGINE_SHA, "accepted Pingo 2 engine identity differs")
    subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor",
                    "31570237a9baa4b8b2312459a655898725b779ef", "HEAD"], check=True)
    official_abi = set(re.findall(r'lib\.get\(b"([^"]+)"\)',
                                  (fab / "src/vdp_interface.rs").read_text()))
    require(official_abi == set(ABI), "smoke ABI must match the official loader exactly")
    return {"fab_tag": "1.2.5", "fab_commit": FAB_COMMIT,
            "userspace_vdp_gl_commit": GL_COMMIT, "crc_version": "1.0.4",
            "crc_source_sha256": closure(crc, [crc / "library.properties"] +
                                        [p for p in (crc / "src").rglob("*") if p.is_file()]),
            "engine_sha256": ENGINE_SHA}


def child(module):
    # Fab leaks its library handle deliberately. Each run owns one process;
    # never dlclose while the upstream shim's detached threads remain alive.
    lib = C.CDLL(str(module), mode=os.RTLD_NOW | os.RTLD_LOCAL)
    for name, (result, arguments) in ABI.items():
        function = getattr(lib, name)
        function.restype, function.argtypes = result, arguments
    lib.setVdpDebugLogging(False)  # forced project probe must not need verbose
    lib.set_startup_screen_mode(0)
    lib.vdp_setup()
    threading.Thread(target=lib.vdp_loop, daemon=True).start()

    class Probe(C.Structure):
        _fields_ = [("render_status", C.c_int), ("drawn_pixels", C.c_uint32),
                    ("depth_pixels", C.c_uint32), ("framebuffer_fnv1a", C.c_uint32),
                    ("depth_fnv1a", C.c_uint32)]

    lib.pingo2_target_probe_run.restype = C.c_int
    lib.pingo2_target_probe_run.argtypes = [C.POINTER(Probe)]
    for _ in range(2):
        probe = Probe()
        require(lib.pingo2_target_probe_run(C.byref(probe)) == 0, "probe failed")
        require([getattr(probe, name) for name, _ in Probe._fields_] ==
                [0, 342, 342, 0xebfd0b25, 0x0c76557b], "probe differs from accepted native contract")

    capacity = 1024 * 768 * 3
    pixels = (C.c_uint8 * (capacity + 32))()
    C.memset(C.addressof(pixels) + capacity, 0xA5, 32)
    width, height, rate = C.c_int(), C.c_int(), C.c_float()

    def capture():
        lib.signal_vblank()
        lib.copyVgaFramebuffer(C.byref(width), C.byref(height), pixels, C.byref(rate))
        require(0 < width.value <= 1024 and 0 < height.value <= 768, "invalid framebuffer size")
        require(math.isfinite(rate.value) and rate.value > 0, "invalid frame rate")
        require(bytes(pixels[capacity:]) == b"\xa5" * 32, "framebuffer overrun")
        return C.string_at(pixels, width.value * height.value * 3)

    def send(values):
        for value in values:
            deadline = time.monotonic() + 2
            while not lib.z80_uart0_is_cts():
                require(time.monotonic() < deadline, "VDP CTS timeout")
                time.sleep(0.001)
            lib.z80_send_to_vdp(value)

    def await_frame(predicate):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            frame = capture()
            if predicate(frame):
                return frame
            time.sleep(0.02)
        raise RuntimeError("VDU commands did not produce the expected live framebuffer")

    # UART capacity is initialized by setup. Do not prequeue the MOS poll:
    # the tagged shim discards writes before its CTS threshold is configured.
    send([23, 0, 0x80, 1])
    received = bytearray()
    value = C.c_uint8()
    deadline = time.monotonic() + 5
    while b"\x80\x01\x01" not in received:
        require(time.monotonic() < deadline, "VDP general-poll reply timeout")
        if lib.z80_recv_from_vdp(C.byref(value)):
            received.append(value.value)
        else:
            time.sleep(0.001)

    # Hide cursor, black background, clear. Then change to a nonblack uniform
    # background. A fallback black buffer, cursor blink or a static boot image
    # cannot satisfy both checks; this exercises the real stock UART/VDU path.
    send([23, 1, 0, 17, 128, 12])
    black = await_frame(lambda frame: not any(frame))
    send([17, 129, 12])
    colour = await_frame(lambda frame: any(frame[:3]) and
                         frame == frame[:3] * (len(frame) // 3))
    require(black != colour, "framebuffer did not change")
    samples = (C.c_uint8 * 64)()
    lib.getAudioSamples(samples, len(samples))
    lib.vdp_shutdown()
    report = {"abi_symbols_resolved": sorted(ABI), "probe_calls": 2,
              "probe": PROBE_RECORD, "width": width.value, "height": height.value,
              "refresh_hz": rate.value, "framebuffer_rgb": list(colour[:3]),
              "general_poll_reply": "800101",
              "black_frame_sha256": hashlib.sha256(black).hexdigest(),
              "colour_frame_sha256": hashlib.sha256(colour).hexdigest(),
              "shutdown": "vdp_shutdown returned; process exit, no dlclose/restart claim"}
    print("PINGO2_SMOKE " + json.dumps(report), flush=True)
    os._exit(0)  # same process-lifetime boundary as Fab, no unsafe C++ teardown


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fab-root", type=Path)
    parser.add_argument("--crc-root", type=Path)
    parser.add_argument("--module", type=Path)
    parser.add_argument("--verify-inputs", action="store_true")
    parser.add_argument("--child", action="store_true")
    args = parser.parse_args()
    if args.child:
        child(args.module.resolve())
    require(args.fab_root is not None and args.crc_root is not None, "supply Fab and CRC roots")
    identities = verify_inputs(args.fab_root.resolve(), args.crc_root.resolve())
    if args.verify_inputs:
        print("Pinned Fab, userspace-vdp-gl, CRC and accepted Pingo 2 source: PASS")
        return
    module = args.module.resolve()
    initial_module_sha = sha(module)
    exports = command("nm", "-D", "--defined-only", str(module))
    exported = {line.split()[-1] for line in exports.splitlines()}
    require(set(ABI) | {"pingo2_target_probe_run"} <= exported, "missing required exports")
    dependencies = command("ldd", str(module))
    require("not found" not in dependencies, "unresolved native dependency")
    runs = []
    for run in range(3):
        started = time.monotonic()
        result = subprocess.run([sys.executable, str(Path(__file__).resolve()),
                                 "--child", "--module", str(module)],
                                capture_output=True, text=True, timeout=20)
        require(result.returncode == 0, f"child failed ({result.returncode}):\n{result.stdout}\n{result.stderr}")
        require(result.stderr.count("Pingo2 target probe:") == 1 and
                PROBE_RECORD in result.stderr, "missing or incorrect forced startup record")
        records = [line.removeprefix("PINGO2_SMOKE ") for line in result.stdout.splitlines()
                   if line.startswith("PINGO2_SMOKE ")]
        require(len(records) == 1, "missing child completion record")
        report = json.loads(records[0])
        require(not runs or report == runs[0]["result"], "nondeterministic fresh-process result")
        runs.append({"seconds": round(time.monotonic() - started, 3), "result": report})
        print(f"Fresh-process module smoke {run + 1}/3: PASS ({runs[-1]['seconds']} s)")
    files = [ROOT / p for p in command("git", "-C", str(ROOT), "ls-files", "video").splitlines()]
    files += [p for p in (ROOT / "userspace").iterdir() if p.is_file()]
    identities.update({"vdp_base_commit": command("git", "-C", str(ROOT), "rev-parse", "HEAD"),
                       "owned_source_sha256": closure(ROOT, files),
                       "owned_source_files": [p.relative_to(ROOT).as_posix() for p in sorted(files)],
                       "build_config": (module.parent / "config").read_text(),
                       "module_sha256": initial_module_sha, "module_bytes": module.stat().st_size,
                       "module_file": command("file", "-b", str(module)),
                       "abi_exports": sorted(set(ABI) | {"pingo2_target_probe_run"}),
                       "ldd": dependencies, "runs": runs})
    require(sha(module) == initial_module_sha, "module changed during qualification")
    # Generated local candidate, NOT a hardware-qualified immutable snapshot.
    # Never silently replace a previously frozen candidate with different bytes.
    candidate = module.parent / "candidate"
    candidate.mkdir(exist_ok=True)
    frozen = candidate / module.name
    receipt = candidate / "qualification.json"
    require(not frozen.exists() or sha(frozen) == initial_module_sha,
            "a different candidate is already frozen; review before replacing it")
    if receipt.exists():
        previous = json.loads(receipt.read_text())
        for key in ["owned_source_sha256", "engine_sha256", "fab_commit",
                    "userspace_vdp_gl_commit", "crc_source_sha256", "build_config"]:
            require(previous[key] == identities[key],
                    f"frozen candidate input changed ({key}); review before replacing it")
    if not frozen.exists():
        shutil.copy2(module, frozen)
    require(sha(frozen) == initial_module_sha, "frozen module copy differs")
    receipt.write_text(json.dumps(identities, indent=2) + "\n")
    print(f"Qualified native candidate: {initial_module_sha}\n{frozen}")


if __name__ == "__main__":
    main()
