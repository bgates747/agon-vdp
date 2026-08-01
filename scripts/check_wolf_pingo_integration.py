#!/usr/bin/env python3
"""Preflight the declared Wolf3DOrig/Pingo VDP integration surface.

The combined worktree is intentionally assembled as uncommitted changes on top
of an exact upstream commit.  Therefore this checker compares the working tree
directly with that base; it does not require a combined integration commit.

Only Python's standard library and Git are required.  Run from the repository
root:

    python3 scripts/check_wolf_pingo_integration.py
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import sys
from typing import Iterable


BASE_COMMIT = "c7ac293d2aa81ddfa693390549bcd909069c8fc3"
PINGO_COMMIT = "c26a0cec30b20ddc619060ec03195ebe81c81dc2"
WOLF_COMMIT = "ae689415e58fd8dbb1265931177d8241669b636e"
WOLF_APP_COMMIT = "3695b7dbfbecb63a635fee332fa4d9a937b9263b"
CONTRACT_COMMIT = "82c01453af433072d6d963d8def4206b79f70e93"

# These are the only reviewed content divergences in exclusive subsystem
# source.  Pinning each complete file prevents a narrow integration exception
# from becoming permission for arbitrary edits.  An intentional follow-up
# must be reviewed and update its fingerprint explicitly.
REVIEWED_PINGO_3D_H_SHA256 = (
    "7780c598ff02550f295c12c4b352a2b11c1dc65ba331dfb7114424ffce72ec7c"
)
REVIEWED_WOLF3D_H_SHA256 = (
    "bc97ba43a8dc024d9e17244e539eb41692363cf14e53fc5a2628ee08aa63c9fb"
)
REVIEWED_WOLF3D_DRAW_H_SHA256 = (
    "1e395f7fbf4f5730d73cb96eaf16646c14a35119080ffb26cebc1254bbecb494"
)
# Test-only companion needed to compile the nominated renderer unit against
# the reviewed GetTile type guard and to exercise reject/recover behavior.
REVIEWED_WOLF_RENDERER_TEST_SHA256 = (
    "85d65e3b1e80566ccad21c1026c3d1ba7b471d36740872043a24c56e93323bec"
)
# The shared dispatcher and cross-subsystem regression are hand-composed
# integration surfaces.  Their complete reviewed contents are pinned just as
# narrowly as the exclusive exceptions above.
REVIEWED_VDU_BUFFERED_H_SHA256 = (
    "9e0c90065cbd8ba353f1bb90af7489b315adcb8579c15946bd8ddee263a4cdda"
)
REVIEWED_COMBINED_SUBSYSTEM_TEST_SHA256 = (
    "2a1bdbdac5bc8f744b4eb61c17cc60f3ff243cc019a294e1b12bb156b4880a74"
)
REVIEWED_EMULATOR_FILES = {
    "docs/wolf-pingo-emulator.md": (
        "4e7e03b4b91f7b3f818883bfc37585b82ec0b38ef0f903a135701cd54f7b14cd"
    ),
    "scripts/run_combined_emulator.sh": (
        "b398985505693b4bf6d6c1e3deeabadc365b20bef2e9a96da61c824ab5133fbd"
    ),
    "scripts/setup_combined_emulator.py": (
        "725f7b7ac8e2faeb80ab9dd95c286ab71614eea1a9a8464af2e2fb5d74301474"
    ),
    "userspace/combined_emulator_exit.asm": (
        "66e40cd06f1986f62b0843b2d129bd8e8ff0496bf34cfe2ef17c4c10e8d779ed"
    ),
}
REVIEWED_COMBINED_README_SHA256 = (
    "af75a5591cf45f214a63be01c4e3c89da4bd96517c0074684ac6afbd531d9a7b"
)

SHARED_PATHS = frozenset(
    {
        "platformio.ini",
        "userspace/Makefile",
        "userspace/arduino_yield_shim.h",
        "video/agon.h",
        "video/agon_fonts.h",
        "video/buffers.h",
        "video/context/cursor.h",
        "video/context/graphics.h",
        "video/hexload.h",
        "video/vdu_audio.h",
        "video/vdu_buffered.h",
        "video/vdu_layers.h",
        "video/vdu_sprites.h",
        "video/vdu_stream_processor.h",
        "video/vdu_sys.h",
        "video/version.h",
        "video/video.ino",
        "video/ymodem.h",
    }
)

# Root/support files changed only by the nominated Pingo source.  They are
# treated as Pingo-owned imports even though they do not fit a pingo_* prefix.
PINGO_SUPPORT_PATHS = frozenset(
    {
        ".gitignore",
        "README.md",
        "userspace/README.md",
        "userspace/bridge_ubsan.supp",
    }
)

# These files are created only to compose and record the combined product.  No
# prefix-wide exception is used: a new integration file must be named here or
# the preflight fails as an unexpected path.
INTEGRATION_ONLY_PATHS = frozenset(
    {
        "docs/devlog-2026-08-01.md",
        "docs/wolf-pingo-integration.md",
        "docs/wolf-pingo-emulator.md",
        "scripts/check_wolf_pingo_integration.py",
        "scripts/run_combined_emulator.sh",
        "scripts/setup_combined_emulator.py",
        "userspace/combined_emulator_exit.asm",
        "userspace/combined_subsystem_test.cpp",
        "userspace/vdp_combined.cpp",
    }
)

# Shared paths whose accepted resolution is an exact source version rather
# than a hand merge.
EXACT_PINGO_SHARED = frozenset(
    {
        "platformio.ini",
        "video/agon_fonts.h",
        "video/buffers.h",
        "video/context/graphics.h",
        "video/vdu_audio.h",
        "video/vdu_sprites.h",
    }
)
EXACT_WOLF_SHARED = frozenset(
    {
        "video/context/cursor.h",
        "video/hexload.h",
        "video/vdu_layers.h",
    }
)
EXACT_COMMON_SHARED = frozenset(
    {
        "userspace/arduino_yield_shim.h",
        "video/ymodem.h",
    }
)

# Wolf's vdu_sys change is intentionally rejected as diagnostic noise.  Its
# combined form must remain byte-for-byte upstream.
BASE_RESOLVED_SHARED = frozenset({"video/vdu_sys.h"})


class Preflight:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.notes: list[str] = []

    def require(self, condition: bool, message: str) -> None:
        if not condition:
            self.errors.append(message)

    def note(self, message: str) -> None:
        self.notes.append(message)


def git(repo: Path, *args: str, check: bool = True) -> bytes:
    command = ["git", "-C", str(repo), *args]
    result = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if check and result.returncode != 0:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"{' '.join(command)} failed: {detail}")
    return result.stdout


def resolved_commit(repo: Path, commit: str) -> str:
    return git(repo, "rev-parse", f"{commit}^{{commit}}").decode().strip()


def commit_file(repo: Path, commit: str, path: str) -> bytes:
    return git(repo, "show", f"{commit}:{path}")


def source_manifest(repo: Path, base: str, tip: str) -> set[str]:
    output = git(
        repo,
        "diff",
        "--name-only",
        "--no-renames",
        "--diff-filter=ACDMRTUXB",
        base,
        tip,
        "--",
    )
    return {
        line
        for line in output.decode("utf-8", errors="strict").splitlines()
        if line
    }


def working_manifest(repo: Path, base: str) -> set[str]:
    tracked = git(
        repo,
        "diff",
        "--name-only",
        "--no-renames",
        base,
        "--",
    ).decode("utf-8", errors="strict").splitlines()
    untracked = git(
        repo,
        "ls-files",
        "--others",
        "--exclude-standard",
        "-z",
    ).decode("utf-8", errors="strict").split("\0")
    return {path for path in [*tracked, *untracked] if path}


def is_pingo_exclusive(path: str) -> bool:
    return (
        path.startswith("video/pingo/")
        or path == "video/pingo_3d.h"
        or path.startswith("userspace/pingo_")
        or path == "userspace/vdp_pingo.cpp"
        or path.startswith("docs/")
        or path.startswith("scripts/")
        or path in PINGO_SUPPORT_PATHS
    )


def is_wolf_exclusive(path: str) -> bool:
    return (
        path.startswith("video/wolf3d/")
        or path == "video/wolf3d.h"
        or path.startswith("userspace/wolf3d_")
        or path == "userspace/vdp_wolf3d.cpp"
    )


def classify_source_manifest(
    preflight: Preflight, name: str, manifest: set[str]
) -> tuple[set[str], set[str]]:
    exclusive: set[str] = set()
    shared: set[str] = set()
    unknown: set[str] = set()

    for path in manifest:
        if path in SHARED_PATHS:
            shared.add(path)
        elif name == "Pingo" and is_pingo_exclusive(path):
            exclusive.add(path)
        elif name == "Wolf3DOrig" and is_wolf_exclusive(path):
            exclusive.add(path)
        else:
            unknown.add(path)

    preflight.require(
        not unknown,
        f"{name} nominated manifest has undeclared paths: {sorted(unknown)}",
    )
    return exclusive, shared


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def reviewed_file(
    preflight: Preflight,
    repo: Path,
    path: str,
    expected_sha256: str,
    description: str,
) -> str:
    candidate = repo / path
    if not candidate.is_file():
        preflight.errors.append(f"missing {description}: {path}")
        return ""
    data = candidate.read_bytes()
    actual_sha256 = sha256(data)
    preflight.require(
        actual_sha256 == expected_sha256,
        f"{path} differs from {description} "
        f"(expected {expected_sha256}, got {actual_sha256})",
    )
    return data.decode("utf-8")


def compare_worktree_file(
    preflight: Preflight,
    combined_repo: Path,
    source_repo: Path,
    source_commit: str,
    path: str,
    label: str,
) -> None:
    worktree_path = combined_repo / path
    if not worktree_path.is_file():
        preflight.errors.append(f"missing {label} import: {path}")
        return
    expected = commit_file(source_repo, source_commit, path)
    actual = worktree_path.read_bytes()
    preflight.require(
        actual == expected,
        f"{label} import differs from {source_commit[:8]}: {path} "
        f"(expected {sha256(expected)}, got {sha256(actual)})",
    )


def compare_to_base(
    preflight: Preflight, combined_repo: Path, path: str
) -> None:
    worktree_path = combined_repo / path
    if not worktree_path.is_file():
        preflight.errors.append(f"missing upstream-resolved shared file: {path}")
        return
    expected = commit_file(combined_repo, BASE_COMMIT, path)
    actual = worktree_path.read_bytes()
    preflight.require(
        actual == expected,
        f"{path} must remain exact VDP 2.16 base; Wolf's debug-only change is rejected",
    )


def require_regex(
    preflight: Preflight,
    text: str,
    pattern: str,
    description: str,
    *,
    count: int | None = None,
) -> None:
    matches = re.findall(pattern, text, flags=re.MULTILINE)
    if count is None:
        preflight.require(bool(matches), f"missing {description}")
    else:
        preflight.require(
            len(matches) == count,
            f"expected {count} occurrence(s) of {description}, found {len(matches)}",
        )


def check_protocol_and_branding(preflight: Preflight, repo: Path) -> None:
    agon = (repo / "video/agon.h").read_text(encoding="utf-8")
    buffered = reviewed_file(
        preflight,
        repo,
        "video/vdu_buffered.h",
        REVIEWED_VDU_BUFFERED_H_SHA256,
        "reviewed shared typed-buffer lifecycle integration",
    )
    version = (repo / "video/version.h").read_text(encoding="utf-8")
    video = (repo / "video/video.ino").read_text(encoding="utf-8")

    require_regex(
        preflight,
        agon,
        r"^\s*#define\s+BUFFERED_PINGO_3D\s+0x49\b",
        "Pingo buffered opcode 0x49",
        count=1,
    )
    require_regex(
        preflight,
        agon,
        r"^\s*#define\s+BUFFERED_WOLF3D\s+0x4A\b",
        "Wolf3DOrig buffered opcode 0x4A",
        count=1,
    )
    require_regex(
        preflight,
        buffered,
        r"\bcase\s+BUFFERED_PINGO_3D\s*:",
        "Pingo buffered dispatcher",
        count=1,
    )
    require_regex(
        preflight,
        buffered,
        r"\bcase\s+BUFFERED_WOLF3D\s*:",
        "Wolf3DOrig buffered dispatcher",
        count=1,
    )

    expected_macros = {
        "VERSION_MAJOR": "2",
        "VERSION_MINOR": "16",
        "VERSION_PATCH": "0",
        "VERSION_CANDIDATE": "0",
        "VERSION_VARIANT": '"Platform"',
        "VERSION_SUBTITLE": '"Bistromathics"',
        "PINGO_VERSION": '"0.1.0 Alpha 1"',
        "WOLF3DORIG_VERSION": '"0.1.0 Alpha 1"',
    }
    for name, value in expected_macros.items():
        require_regex(
            preflight,
            version,
            rf"^\s*#define\s+{name}\s+{re.escape(value)}(?:\s|$)",
            f"branding constant {name}={value}",
            count=1,
        )

    upstream = 'printFmt("Agon %s VDP Version %d.%d.%d"'
    pingo = 'printFmt("Pingo %s\\n\\r", PINGO_VERSION);'
    wolf = 'printFmt("Wolf3DOrig %s\\n\\r", WOLF3DORIG_VERSION);'
    upstream_at = video.find(upstream)
    pingo_at = video.find(pingo)
    wolf_at = video.find(wolf)
    preflight.require(upstream_at >= 0, "missing upstream boot-branding print")
    preflight.require(pingo_at >= 0, "missing separate Pingo boot-branding line")
    preflight.require(wolf_at >= 0, "missing separate Wolf3DOrig boot-branding line")
    preflight.require(
        0 <= upstream_at < pingo_at < wolf_at,
        "boot branding must print upstream, Pingo, then Wolf3DOrig identities",
    )


def check_reviewed_isolation_resolutions(
    preflight: Preflight, repo: Path
) -> None:
    pingo = reviewed_file(
        preflight,
        repo,
        "video/pingo_3d.h",
        REVIEWED_PINGO_3D_H_SHA256,
        "reviewed Pingo lifetime/reentrancy resolution",
    )
    wolf = reviewed_file(
        preflight,
        repo,
        "video/wolf3d.h",
        REVIEWED_WOLF3D_H_SHA256,
        "reviewed Wolf scratch/transaction resolution",
    )
    wolf_draw = reviewed_file(
        preflight,
        repo,
        "video/wolf3d/render/wolf3d_draw.h",
        REVIEWED_WOLF3D_DRAW_H_SHA256,
        "reviewed Wolf tilemap type-safety resolution",
    )
    wolf_renderer_test = reviewed_file(
        preflight,
        repo,
        "userspace/wolf3d_renderer_test.cpp",
        REVIEWED_WOLF_RENDERER_TEST_SHA256,
        "reviewed Wolf tilemap test adaptation",
    )
    wolf_smoke = (repo / "userspace/wolf3d_smoke.cpp").read_text(
        encoding="utf-8"
    )
    wolf_readme = (repo / "video/wolf3d/README.md").read_text(
        encoding="utf-8"
    )
    buffered = (repo / "video/vdu_buffered.h").read_text(encoding="utf-8")

    forbidden_scratch = (
        "WOLF3D_SCRATCH_WALL_BUFFER_ID",
        "WOLF3D_SCRATCH_SPRITE_BUFFER_ID",
        "WOLF3D_SCRATCH_WEAPON_BUFFER_ID",
        "processor.bufferClear(",
        "processor.bufferCreate(",
        "processor.createBitmapFromBuffer(",
    )
    for token in forbidden_scratch:
        preflight.require(
            token not in wolf,
            f"C-015 regression: forbidden global scratch operation remains: {token}",
        )
    preflight.require(
        wolf.count("BufferStream scratch(") == 3,
        "C-015 resolution must use three caller-owned BufferStream scratch paths",
    )
    preflight.require(
        wolf.count("PixelFormat::RGBA2222") >= 3,
        "C-015 resolution must wrap private scratch as local RGBA2222 bitmaps",
    )

    # C-025 was explicitly deferred. The combined product retains the
    # nominated Wolf protocol and must not silently acquire the proposed W3DV
    # version query or claim opcode-local subcommand 40.
    wolf_protocol_surface = "\n".join(
        (wolf, wolf_smoke, wolf_readme, buffered)
    )
    preflight.require(
        "W3DV" not in wolf_protocol_surface,
        "C-025 regression: deferred W3DV query was reintroduced",
    )
    preflight.require(
        re.search(r"\bcase\s+40\s*:", wolf_protocol_surface) is None,
        "C-025 regression: deferred Wolf subcommand 40 was reintroduced",
    )
    require_regex(
        preflight,
        buffered,
        r"^\s*return\s+subcommand\s*<=\s*17\s*\|\|\s*subcommand\s*==\s*41\s*;",
        "nominated Wolf subcommand allowlist without deferred subcommand 40",
        count=1,
    )

    processor = (repo / "video/vdu_stream_processor.h").read_text(
        encoding="utf-8"
    )
    lifecycle_required = (
        "using Wolf3dControlRegistry",
        "wolf3dControlRegistry()",
        "isKnownWolf3dSubcommand",
        "bufferDeinitializeWolf3D(uint16_t bufferId)",
        "wolf3dControlRegistry().clear();",
        "bufferDeinitializeWolf3D(bufferId);",
        "make_shared_psram<Wolf3dControl>()",
    )
    for token in lifecycle_required:
        preflight.require(
            token in buffered,
            f"missing reviewed Wolf lifecycle mechanism: {token}",
        )
    preflight.require(
        "void bufferDeinitializeWolf3D(uint16_t bufferId);" in processor,
        "VDUStreamProcessor is missing the Wolf erase-by-ID lifecycle hook",
    )
    lifecycle_forbidden = (
        "static std::map<uint16_t, Wolf3dControl> wolf3dControls",
        "wolf3dControls[bufferId]",
    )
    for token in lifecycle_forbidden:
        preflight.require(
            token not in buffered,
            f"Wolf lifecycle regression: implicit/unbounded registry remains: {token}",
        )

    function_at = buffered.find("void VDUStreamProcessor::bufferUseWolf3D")
    function_text = buffered[function_at:] if function_at >= 0 else ""
    known_at = function_text.find("isKnownWolf3dSubcommand")
    lookup_at = function_text.find("wolf3dControlRegistry()")
    existing_at = function_text.find("if (controlIter != controls.end())")
    pin_at = function_text.find("auto control = controlIter->second;", existing_at)
    existing_handle_at = function_text.find("control->handle_subcommand(", pin_at)
    allocate_at = function_text.find("make_shared_psram<Wolf3dControl>()")
    new_handle_at = function_text.find("control->handle_subcommand(", allocate_at)
    clear_at = function_text.find("bufferClear(bufferId);", new_handle_at)
    publish_at = function_text.find(
        "controls.emplace(bufferId, std::move(control));", clear_at
    )
    preflight.require(function_at >= 0, "missing bufferUseWolf3D implementation")
    preflight.require(
        0 <= known_at < lookup_at,
        "Wolf opcode-local subcommand number must be checked before registry lookup",
    )
    preflight.require(
        0 <= existing_at < pin_at < existing_handle_at,
        "an existing Wolf handler must hold a local shared_ptr pin",
    )
    preflight.require(
        0 <= allocate_at < new_handle_at < clear_at < publish_at,
        "new Wolf state must parse privately before bufferClear/registry publication",
    )
    for token in (
        "static bool read_long(VDUStreamProcessor& processor, int32_t& value)",
        "bool handle_subcommand(VDUStreamProcessor& processor, uint8_t subcmd)",
        "case 41: return set_render_notification(processor);",
    ):
        preflight.require(
            token in wolf,
            f"missing reviewed Wolf transactional parser mechanism: {token}",
        )
    preflight.note(
        "reviewed Wolf publication: a prospective control may be allocated "
        "before parsing, but no persistent state or ID mutation precedes a "
        "complete payload"
    )

    # A first ordinary write over Pingo must remove its raw in-place object
    # block. Ordinary-to-ordinary writes retain the upstream multi-block append
    # contract, while a Wolf-only occupant needs registry teardown but has no
    # ordinary block to clear.
    write_at = buffered.find("uint32_t VDUStreamProcessor::bufferWrite")
    write_end = buffered.find("void VDUStreamProcessor::bufferCall", write_at)
    buffer_write = buffered[write_at:write_end]
    remaining_at = buffer_write.find("if (remaining > 0)")
    pingo_guard_at = buffer_write.find("if (isPingo3dControlBuffer(bufferId))")
    pingo_clear_at = buffer_write.find("bufferClear(bufferId);", pingo_guard_at)
    ordinary_else_at = buffer_write.find("} else {", pingo_clear_at)
    wolf_clear_at = buffer_write.find(
        "bufferDeinitializeWolf3D(bufferId);", ordinary_else_at
    )
    append_at = buffer_write.find(
        "buffers[bufferId].push_back(std::move(bufferStream));", wolf_clear_at
    )
    preflight.require(
        0
        <= remaining_at
        < pingo_guard_at
        < pingo_clear_at
        < ordinary_else_at
        < wolf_clear_at
        < append_at,
        "bufferWrite must clear only a replaced Pingo raw block, then preserve "
        "ordinary append semantics",
    )
    preflight.require(
        buffer_write.count("bufferClear(bufferId);") == 1,
        "bufferWrite contains an unexpected unconditional/additional buffer clear",
    )

    # Pingo creation is a transactional type replacement. Consume and
    # validate the complete dimensions first, build the candidate privately,
    # and only then clear the prior occupant and publish the staged control.
    pingo_use_at = buffered.find("void VDUStreamProcessor::bufferUsePingo3D")
    wolf_use_at = buffered.find("void VDUStreamProcessor::bufferUseWolf3D", pingo_use_at)
    pingo_use = buffered[pingo_use_at:wolf_use_at]
    width_at = pingo_use.find("auto width = readWord_t();")
    height_at = pingo_use.find("auto height = readWord_t();", width_at)
    validate_at = pingo_use.find("width > INT16_MAX || height > INT16_MAX", height_at)
    stage_at = pingo_use.find(
        "make_shared_psram<WritableBufferStream>(sizeof(Pingo3dControl))",
        validate_at,
    )
    construct_at = pingo_use.find(
        "new (storage->getBuffer()) Pingo3dControl()", stage_at
    )
    initialize_at = pingo_use.find("control->initialize(", construct_at)
    commit_clear_at = pingo_use.find("bufferClear(bufferId);", initialize_at)
    publish_at = pingo_use.find(
        "buffers[bufferId].push_back(std::move(storage));", commit_clear_at
    )
    register_at = pingo_use.find(
        "pingo3dControlBuffers.insert(bufferId);", publish_at
    )
    preflight.require(
        0
        <= width_at
        < height_at
        < validate_at
        < stage_at
        < construct_at
        < initialize_at
        < commit_clear_at
        < publish_at
        < register_at,
        "Pingo create must parse, validate, stage, and initialize before "
        "canonical clear/publication",
    )

    # Generic writable creation must likewise allocate before destroying a
    # typed-only occupant; OOM cannot erase a live Wolf control.
    create_at = buffered.find(
        "std::shared_ptr<WritableBufferStream> "
        "VDUStreamProcessor::bufferCreate"
    )
    create_end = buffered.find("void VDUStreamProcessor::setOutputStream", create_at)
    create = buffered[create_at:create_end]
    create_allocate_at = create.find(
        "make_shared_psram<WritableBufferStream>(size)"
    )
    create_failure_at = create.find("if (!buffer || !buffer->getBuffer())")
    create_pingo_clear_at = create.find(
        "bufferDeinitializePingo3D(bufferId);", create_failure_at
    )
    create_wolf_clear_at = create.find(
        "bufferDeinitializeWolf3D(bufferId);", create_pingo_clear_at
    )
    create_publish_at = create.find(
        "buffers[bufferId].push_back(buffer);", create_wolf_clear_at
    )
    preflight.require(
        0
        <= create_allocate_at
        < create_failure_at
        < create_pingo_clear_at
        < create_wolf_clear_at
        < create_publish_at,
        "generic bufferCreate must allocate successfully before typed-state "
        "teardown/publication",
    )

    # Pingo lives in raw BufferStream storage. Placement-new begins its formal
    # lifetime, both failure and teardown paths end it explicitly, and the
    # exclusive bridge pins the representation assumptions with type traits.
    for token in (
        "#include <type_traits>",
        "std::is_trivially_copyable<Pingo3dControl>::value",
        "std::is_trivially_destructible<Pingo3dControl>::value",
    ):
        preflight.require(
            token in pingo,
            f"missing reviewed Pingo raw-storage invariant: {token}",
        )
    preflight.require(
        "new (storage->getBuffer()) Pingo3dControl();" in buffered,
        "Pingo raw storage does not begin object lifetime with placement-new",
    )
    preflight.require(
        buffered.count("control->~Pingo3dControl();") >= 2,
        "Pingo failure and teardown paths must invoke the in-place destructor",
    )

    # General VDP callbacks are reentrant. Snapshot callback IDs before calls,
    # and keep Pingo's completion send as the final member action in the
    # diagnostic render path because a callback may destroy this control.
    callbacks_at = buffered.find("void VDUStreamProcessor::bufferCallCallbacks")
    callbacks_end = buffered.find("// VDU 23, 0, &A0", callbacks_at)
    callbacks = buffered[callbacks_at:callbacks_end]
    preflight.require(
        "std::vector<uint16_t> callbackIds(" in callbacks
        and "for (const auto bufferId : callbackIds)" in callbacks,
        "callback dispatch must iterate a stable callback-ID snapshot",
    )
    completion_comment_at = pingo.find(
        "Completion callbacks may synchronously clear or repurpose this control."
    )
    completion_at = pingo.find(
        "send_render_complete(sequence);", completion_comment_at
    )
    diagnostic_else_at = pingo.find("#else", completion_at)
    preflight.require(
        0 <= completion_comment_at < completion_at < diagnostic_else_at,
        "diagnostic Pingo completion must remain the final reentrant member action",
    )

    # Preserve upstream's read-only debug lookup. operator[] would create a
    # ghost ordinary buffer beside a Wolf-only control and block reciprocal ID
    # replacement.
    vdu_sys = (repo / "video/vdu_sys.h").read_text(encoding="utf-8")
    print_at = vdu_sys.find("void VDUStreamProcessor::printBuffer")
    print_end = vdu_sys.find("void VDUStreamProcessor::sendTime", print_at)
    print_buffer = vdu_sys[print_at:print_end]
    preflight.require(
        "buffers.find(bufferId)" in print_buffer,
        "debug printBuffer must use a read-only find",
    )
    preflight.require(
        "buffers[bufferId]" not in print_buffer,
        "debug printBuffer must not materialize a buffer with operator[]",
    )

    # Wolf tests the referenced tilemap's current type on every use. This
    # rejects Pingo object bytes yet automatically recovers if the same ID is
    # later recreated as an ordinary tilemap.
    tile_guard_at = wolf_draw.find(
        "if (isPingo3dControlBuffer(m_world.tilemapBufferId))"
    )
    tile_find_at = wolf_draw.find(
        "auto it = buffers.find(m_world.tilemapBufferId);", tile_guard_at
    )
    preflight.require(
        0 <= tile_guard_at < tile_find_at,
        "Wolf GetTile must reject live Pingo controls before reading buffers",
    )
    preflight.require(
        "uint8_t UserspaceGetTile(int tilex, int tiley) const" in wolf_draw,
        "missing read-only userspace hook for the real Wolf GetTile path",
    )
    for token in (
        "testPingoControlCannotMasqueradeAsTilemap",
        "pingo3dControlBuffers.insert(kTilemapBufferId);",
        "pingo3dControlBuffers.erase(kTilemapBufferId);",
        'expect("ordinary tilemap recovery"',
    ):
        preflight.require(
            token in wolf_renderer_test,
            f"Wolf renderer test is missing tilemap reject/recover coverage: {token}",
        )

    tests = reviewed_file(
        preflight,
        repo,
        "userspace/combined_subsystem_test.cpp",
        REVIEWED_COMBINED_SUBSYSTEM_TEST_SHA256,
        "reviewed cross-subsystem regression",
    )
    if tests:
        test_evidence = (
            "testDistinctIdsAndFormerScratchIsolation",
            "testWolfTilemapTypeIsolation",
            "testTransactionalPingoCreation",
            "invalid Pingo create damaged ordinary buffer contents/layout",
            "allocation-failed Pingo create damaged ordinary buffer state",
            "invalid Pingo recreation damaged an existing Pingo control",
            "invalid Pingo create damaged a live Wolf occupant",
            "allocation-failed Pingo create damaged a live Wolf occupant",
            "successful Wolf-to-Pingo replacement was not atomic",
            "Wolf interpreted a live Pingo control as tilemap bytes",
            "Wolf did not recover after ordinary tilemap recreation",
            "truncated known Wolf command published control state",
            "completion callback did not clear its active Pingo control",
            "debug inspection damaged the Wolf-only control",
            "ordinary replacement did not remove Pingo's raw backing block",
            "typed replacement fix broke ordinary multi-block append semantics",
            "ordinary replacement retained a stale Pingo backing block",
            "global clear leaked Pingo-owned allocations",
        )
        for token in test_evidence:
            preflight.require(
                token in tests,
                f"combined test is missing reviewed isolation coverage: {token}",
            )

    preflight.note(
        "reviewed exclusive fingerprints: "
        f"pingo_3d.h={REVIEWED_PINGO_3D_H_SHA256}, "
        f"wolf3d.h={REVIEWED_WOLF3D_H_SHA256}, "
        f"wolf3d_draw.h={REVIEWED_WOLF3D_DRAW_H_SHA256}"
    )
    preflight.note(
        "reviewed test-only Wolf renderer adaptation: "
        f"sha256={REVIEWED_WOLF_RENDERER_TEST_SHA256}"
    )
    preflight.note(
        "reviewed shared integration fingerprints: "
        f"vdu_buffered.h={REVIEWED_VDU_BUFFERED_H_SHA256}, "
        "combined_subsystem_test.cpp="
        f"{REVIEWED_COMBINED_SUBSYSTEM_TEST_SHA256}"
    )
    preflight.note(
        "reviewed reentrancy/type-safety coverage: callback snapshot, "
        "Pingo completion-last, raw lifetime and transactional replacement, "
        "read-only debug lookup, and use-time Wolf tilemap guard"
    )


def check_reviewed_emulator_profile(preflight: Preflight, repo: Path) -> None:
    contents = {
        path: reviewed_file(
            preflight,
            repo,
            path,
            expected,
            "reviewed combined-emulator integration file",
        )
        for path, expected in REVIEWED_EMULATOR_FILES.items()
    }
    setup = contents["scripts/setup_combined_emulator.py"]
    launcher = contents["scripts/run_combined_emulator.sh"]
    exit_fixture = contents["userspace/combined_emulator_exit.asm"]
    documentation = contents["docs/wolf-pingo-emulator.md"]

    setup_tokens = (
        'MYSTUFF_ROOT / "agon-dev-env/emulators/wolf-pingo-v2.16"',
        'DEFAULT_MODULE = SOURCE_ROOT / "video/build/userspace/vdp_combined.so"',
        'STANDALONE_PINGO_PROFILE = DEFAULT_PINGOASM_ROOT / "emulator"',
        'STANDALONE_WOLF_PROFILE = DEFAULT_WOLF_ROOT / "agonport/emulator"',
        "def paths_overlap(first: Path, second: Path) -> bool:",
        "validate_profile_location(",
        '"combined VDP source tree", SOURCE_ROOT',
        '"Fab source tree", fab_root',
        '"Pingo application tree", pingoasm_root',
        '"Wolf3DOrig application tree", wolf_root',
        '"standalone Pingo emulator profile", STANDALONE_PINGO_PROFILE',
        '"standalone Wolf emulator profile", STANDALONE_WOLF_PROFILE',
        '"wolf3d.bin": "2d845e2882ae22012f0c64c7705d47b7413dc6b6301bc07bcdfdfadbe635f1c7"',
        '"tiles.agnb": "9990d79e3ae47d0c940e072f70f821d505d9e39ce21f713b38f5d8c7237502c8"',
        '"sprites.agnb": "7bf0f04a6fad56d5a83b26b30f5c0ea3bbafb4c8727a3c5f9d266c30c0e058f9"',
        '"hud.agnb": "1848900559fe9f684dbfc8ae80b0416132db5414bc1ab41f9dad47a5ab26a74d"',
        '"sfx.agnb": "f294c1152bedca56e726e115ce1d1dc2f5aa6191e340a96a79b1e845296f1cd4"',
        'b"SET KEYBOARD 1\\r\\n"',
        'b"cd /mystuff/pingoasm/apps/earth-party-flat/tgt\\r\\n"',
        'b"cd /wolf3d\\r\\n"',
        '"--refresh-autoexec"',
        "if not path.exists() or refresh:",
        'combined_smoke_source = SOURCE_ROOT / "userspace/combined_emulator_exit.asm"',
        'profile / "profile-checksums.sha256"',
        'mos_map = mos.with_suffix(".map")',
        "pingo_visual_files = checked_directory_files(pingo_visual.parent)",
        "pingo_smoke_files = checked_directory_files(pingo_smoke_dir)",
        'f"mos_map_sha256={sha256(mos_map)}"',
        "*pingo_visual_files",
        "*pingo_smoke_files",
        'accepted_combined_snapshots = DEFAULT_PROFILE / "snapshots/wolf3d"',
        'accepted_combined_snapshots / "wolf3d.bin"',
        "accepted_combined_snapshots / name",
        "snapshot_file(source, wolf_snapshot_paths[name])",
    )
    for token in setup_tokens:
        preflight.require(
            token in setup,
            f"combined-emulator setup is missing reviewed invariant: {token}",
        )

    launcher_tokens = (
        'default_profile="${HOME}/Agon/mystuff/agon-dev-env/emulators/wolf-pingo-v2.16"',
        'vdp_module="$(readlink -f -- "${profile}/vdp_combined.so")"',
        'mos_bin="$(readlink -f -- "${profile}/mos_console8.bin")"',
        'mos_map="${mos_bin%.bin}.map"',
        'profile_checksums="${profile}/profile-checksums.sha256"',
        'manifest_snapshot="${run_root}/profile-checksums.sha256"',
        'sha256sum --status --check "${profile_checksums}"',
        'run_root="$(mktemp -d "${TMPDIR:-/tmp}/fab-wolf-pingo.XXXXXX")"',
        "snapshot_launch_artifact()",
        "snapshot_fixture_directory()",
        "snapshot_mutable_file()",
        'launch_artifacts="${run_root}/launch-artifacts"',
        '"${launch_artifacts}/vdp_combined.so"',
        '"${launch_artifacts}/mos_console8.map"',
        '"${profile_sd}/mystuff/pingoasm/apps/earth-party-flat/tgt"',
        '"${profile_sd}/pingo/cube-rgba2222/tgt"',
        '"${profile_sd}/wolf3d" "${sdcard}/wolf3d"',
        '"${profile_sd}/combined-smoke" "${sdcard}/combined-smoke"',
        '"${profile_sd}/autoexec.txt" "${sdcard}/autoexec.txt"',
        "SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy",
        "COMBINED_EMULATOR_SMOKE_LOG",
        "validate_smoke_log_destination()",
        'if [[ -L "${requested}" || -e "${requested}" ]]',
        "retain_smoke_log()",
        'mktemp --tmpdir="${destination_parent}"',
        'ln -- "${temporary}" "${destination}"',
        '"${fab_bin}"',
        '--vdp "${vdp_module}"',
        '--mos "${mos_bin}"',
        "'cd /pingo/cube-rgba2222/tgt'",
        "'load wolf-dispatch-exit.bin'",
        "pingo_render_count=",
        'if [[ "${pingo_render_count}" != 36 ]]',
        "initialize: pingo creating control structure",
        "Wolf3D: Hello from Castle Wolfenstein 3D!",
        "if ((result != 0)); then",
    )
    for token in launcher_tokens:
        preflight.require(
            token in launcher,
            f"combined-emulator launcher is missing reviewed invariant: {token}",
        )

    for token in (
        "db 23,0,0A0h",
        "db 04Ah,0",
        "ld de,60",
        "xor a",
        "out (0),a",
    ):
        preflight.require(
            token in exit_fixture,
            f"combined-emulator exit fixture is missing reviewed behavior: {token}",
        )

    for token in (
        "does not replace or modify either enduring standalone profile",
        "profile-checksums.sha256",
        "The two-way overlap check also rejects a broad parent",
        "freezes the manifest",
        "launches only those copies",
        "either selected Pingo fixture directory",
        "MOS image and symbol map",
        "Every fallback is read-only and must match the pinned hash",
        "COMBINED_EMULATOR_SMOKE_LOG",
        "exact-output oracle",
        "atomic no-overwrite hard link",
        "explicit Pingo initialization, exactly 36",
        "interactive Author visual/audio gates passed",
        "emulator-facing changes are not committed or",
    ):
        preflight.require(
            token in documentation,
            f"combined-emulator documentation is missing reviewed policy: {token}",
        )

    preflight.note(
        "reviewed combined-emulator additions: isolated profile, pinned "
        "artifacts with hash-checked accepted-snapshot recovery, fail-closed "
        "profile overlap rejection, complete launch-input freezing, atomic "
        "no-overwrite log retention, 36-frame Pingo/Wolf headless smoke, "
        "and port-0 clean exit"
    )


def parse_args(argv: Iterable[str]) -> argparse.Namespace:
    home = Path.home()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo",
        type=Path,
        default=Path.cwd(),
        help="combined worktree (default: current directory)",
    )
    parser.add_argument(
        "--pingo-repo",
        type=Path,
        default=home / "Agon/mystuff/agon-vdp-pingo-v216-promotion",
        help="qualified Pingo 2.16 worktree",
    )
    parser.add_argument(
        "--wolf-repo",
        type=Path,
        default=home / "Agon/mystuff/agon-vdp-wolf3d",
        help="nominated Wolf3DOrig VDP repository",
    )
    parser.add_argument(
        "--wolf-app-repo",
        type=Path,
        default=home / "Agon/mystuff/Wolf3dOrig",
        help="nominated Wolf3DOrig application repository",
    )
    parser.add_argument(
        "--contract-repo",
        type=Path,
        default=home / "Agon/mystuff/agon-dev-env",
        help="canonical contract repository",
    )
    return parser.parse_args(list(argv))


def main(argv: Iterable[str] = ()) -> int:
    args = parse_args(argv)
    repo = args.repo.resolve()
    pingo_repo = args.pingo_repo.expanduser().resolve()
    wolf_repo = args.wolf_repo.expanduser().resolve()
    wolf_app_repo = args.wolf_app_repo.expanduser().resolve()
    contract_repo = args.contract_repo.expanduser().resolve()
    preflight = Preflight()

    try:
        top = Path(git(repo, "rev-parse", "--show-toplevel").decode().strip())
        preflight.require(
            top.resolve() == repo,
            f"run from the combined repository root (resolved root: {top})",
        )

        identities = (
            ("combined base", repo, BASE_COMMIT),
            ("Pingo base", pingo_repo, BASE_COMMIT),
            ("Pingo source", pingo_repo, PINGO_COMMIT),
            ("Wolf base", wolf_repo, BASE_COMMIT),
            ("Wolf source", wolf_repo, WOLF_COMMIT),
            ("Wolf application", wolf_app_repo, WOLF_APP_COMMIT),
            ("accepted contract", contract_repo, CONTRACT_COMMIT),
        )
        for label, identity_repo, expected in identities:
            actual = resolved_commit(identity_repo, expected)
            preflight.require(
                actual == expected,
                f"{label} hash mismatch: expected {expected}, got {actual}",
            )
            preflight.note(f"{label}: {actual}")

        pingo_manifest = source_manifest(pingo_repo, BASE_COMMIT, PINGO_COMMIT)
        wolf_manifest = source_manifest(wolf_repo, BASE_COMMIT, WOLF_COMMIT)
        pingo_exclusive, pingo_shared = classify_source_manifest(
            preflight, "Pingo", pingo_manifest
        )
        wolf_exclusive, wolf_shared = classify_source_manifest(
            preflight, "Wolf3DOrig", wolf_manifest
        )
        preflight.require(
            not (pingo_exclusive & wolf_exclusive),
            "nominated exclusive manifests overlap: "
            f"{sorted(pingo_exclusive & wolf_exclusive)}",
        )
        preflight.note(
            "Pingo manifest: "
            f"{len(pingo_manifest)} paths "
            f"({len(pingo_exclusive)} exclusive, {len(pingo_shared)} shared)"
        )
        preflight.note(
            "Wolf3DOrig manifest: "
            f"{len(wolf_manifest)} paths "
            f"({len(wolf_exclusive)} exclusive, {len(wolf_shared)} shared)"
        )

        working = working_manifest(repo, BASE_COMMIT)
        allowed = pingo_manifest | wolf_manifest | INTEGRATION_ONLY_PATHS
        unexpected = working - allowed
        preflight.require(
            not unexpected,
            "combined worktree has undeclared paths: " + str(sorted(unexpected)),
        )

        # A new file within either exclusive namespace is always a fence
        # violation, even if a future broad support allowlist is introduced.
        pingo_fence_crossings = {
            path
            for path in working
            if is_pingo_exclusive(path)
            and path not in pingo_manifest
            and path not in INTEGRATION_ONLY_PATHS
        }
        wolf_fence_crossings = {
            path
            for path in working
            if is_wolf_exclusive(path) and path not in wolf_manifest
        }
        preflight.require(
            not pingo_fence_crossings,
            "undeclared edit crossed the Pingo ownership fence: "
            f"{sorted(pingo_fence_crossings)}",
        )
        preflight.require(
            not wolf_fence_crossings,
            "undeclared edit crossed the Wolf ownership fence: "
            f"{sorted(wolf_fence_crossings)}",
        )

        for path in sorted(pingo_exclusive):
            if path == "README.md":
                readme = (repo / path).read_bytes()
                preflight.require(
                    sha256(readme) == REVIEWED_COMBINED_README_SHA256,
                    "README.md differs from the reviewed combined-product "
                    "introduction "
                    f"(expected {REVIEWED_COMBINED_README_SHA256}, "
                    f"got {sha256(readme)})",
                )
                continue
            if path == "video/pingo_3d.h":
                # Reviewed integration-only raw-lifetime and completion-order
                # changes are fingerprinted and checked semantically below.
                continue
            compare_worktree_file(
                preflight, repo, pingo_repo, PINGO_COMMIT, path, "Pingo-exclusive"
            )
        reviewed_wolf_exclusive = {
            "userspace/wolf3d_renderer_test.cpp",
            "video/wolf3d.h",
            "video/wolf3d/render/wolf3d_draw.h",
        }
        for path in sorted(wolf_exclusive - reviewed_wolf_exclusive):
            compare_worktree_file(
                preflight,
                repo,
                wolf_repo,
                WOLF_COMMIT,
                path,
                "Wolf3DOrig-exclusive",
            )

        for path in sorted(EXACT_PINGO_SHARED):
            compare_worktree_file(
                preflight, repo, pingo_repo, PINGO_COMMIT, path, "Pingo shared"
            )
        for path in sorted(EXACT_WOLF_SHARED):
            compare_worktree_file(
                preflight, repo, wolf_repo, WOLF_COMMIT, path, "Wolf shared"
            )
        for path in sorted(EXACT_COMMON_SHARED):
            pingo_bytes = commit_file(pingo_repo, PINGO_COMMIT, path)
            wolf_bytes = commit_file(wolf_repo, WOLF_COMMIT, path)
            actual_path = repo / path
            if not actual_path.is_file():
                preflight.errors.append(f"missing common shared import: {path}")
                continue
            actual = actual_path.read_bytes()
            preflight.require(
                actual in {pingo_bytes, wolf_bytes},
                f"common shared file matches neither nominated source: {path}",
            )
            if path == "userspace/arduino_yield_shim.h":
                shim = actual.decode("utf-8")
                for token in ("#include <thread>", "inline void yield()", "std::this_thread::yield()"):
                    preflight.require(
                        token in shim,
                        f"common Arduino yield shim is missing required behavior: {token}",
                    )
        for path in sorted(BASE_RESOLVED_SHARED):
            compare_to_base(preflight, repo, path)

        required_combined_shared = (
            (pingo_shared | wolf_shared) - BASE_RESOLVED_SHARED
        )
        missing_shared = required_combined_shared - working
        preflight.require(
            not missing_shared,
            "combined worktree is missing declared shared integrations: "
            f"{sorted(missing_shared)}",
        )

        check_protocol_and_branding(preflight, repo)
        check_reviewed_isolation_resolutions(preflight, repo)
        check_reviewed_emulator_profile(preflight, repo)
        preflight.note(f"combined working manifest: {len(working)} paths")

    except (OSError, RuntimeError, UnicodeError) as exc:
        preflight.errors.append(str(exc))

    print("Wolf3DOrig/Pingo integration preflight")
    for note in preflight.notes:
        print(f"  OK  {note}")
    if preflight.errors:
        for error in preflight.errors:
            print(f"  FAIL {error}", file=sys.stderr)
        print(
            f"preflight failed with {len(preflight.errors)} error(s)",
            file=sys.stderr,
        )
        return 1

    print(
        "PASS: exact inputs, ownership manifests, shared resolutions, "
        "opcodes, branding, and reviewed isolation fixes are consistent"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
