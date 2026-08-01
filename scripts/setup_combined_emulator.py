#!/usr/bin/env python3
"""Provision the isolated mutable Wolf3DOrig + Pingo Fab profile."""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


HOME = Path.home()
AGON_ROOT = HOME / "Agon"
MYSTUFF_ROOT = AGON_ROOT / "mystuff"
SOURCE_ROOT = Path(__file__).resolve().parent.parent

DEFAULT_PROFILE = (
    MYSTUFF_ROOT / "agon-dev-env/emulators/wolf-pingo-v2.16"
)
DEFAULT_FAB_ROOT = MYSTUFF_ROOT / "fab-agon-emulator"
DEFAULT_MODULE = SOURCE_ROOT / "video/build/userspace/vdp_combined.so"
DEFAULT_PINGOASM_ROOT = MYSTUFF_ROOT / "pingoasm"
DEFAULT_WOLF_ROOT = MYSTUFF_ROOT / "Wolf3dOrig"
STANDALONE_PINGO_PROFILE = DEFAULT_PINGOASM_ROOT / "emulator"
STANDALONE_WOLF_PROFILE = DEFAULT_WOLF_ROOT / "agonport/emulator"

NOMINATED_WOLF_HASHES = {
    "wolf3d.bin": "2d845e2882ae22012f0c64c7705d47b7413dc6b6301bc07bcdfdfadbe635f1c7",
    "tiles.agnb": "9990d79e3ae47d0c940e072f70f821d505d9e39ce21f713b38f5d8c7237502c8",
    "sprites.agnb": "7bf0f04a6fad56d5a83b26b30f5c0ea3bbafb4c8727a3c5f9d266c30c0e058f9",
    "hud.agnb": "1848900559fe9f684dbfc8ae80b0416132db5414bc1ab41f9dad47a5ab26a74d",
    "sfx.agnb": "f294c1152bedca56e726e115ce1d1dc2f5aa6191e340a96a79b1e845296f1cd4",
}

VISUAL_AUTOEXEC = (
    b"SET KEYBOARD 1\r\n"
    b"cd /mystuff/pingoasm/apps/earth-party-flat-local/tgt\r\n"
    b"load earth-party-flat.bin\r\n"
    b"run\r\n"
    b"cd /wolf3d\r\n"
    b"load wolf3d.bin\r\n"
    b"run\r\n"
)


def lexical_path(path: Path) -> Path:
    """Return an absolute normalized path without following symlinks."""

    return Path(os.path.abspath(os.fspath(path.expanduser())))


def paths_overlap(first: Path, second: Path) -> bool:
    """Return whether either path is equal to or contains the other."""

    return (
        first == second
        or first in second.parents
        or second in first.parents
    )


def validate_profile_location(
    profile_argument: Path,
    profile: Path,
    *,
    fab_root: Path,
    pingoasm_root: Path,
    wolf_root: Path,
) -> None:
    """Reject a profile that could overwrite or recursively map source data."""

    profile_lexical = lexical_path(profile_argument)
    if profile_lexical.is_symlink():
        raise SystemExit(
            f"Refusing a symlink as the combined profile path: "
            f"{profile_lexical}"
        )

    protected = (
        ("combined VDP source tree", SOURCE_ROOT),
        ("Fab source tree", fab_root),
        ("Pingo application tree", pingoasm_root),
        ("Wolf3DOrig application tree", wolf_root),
        ("standalone Pingo emulator profile", STANDALONE_PINGO_PROFILE),
        ("standalone Wolf emulator profile", STANDALONE_WOLF_PROFILE),
    )
    violations: list[str] = []
    for label, protected_path in protected:
        protected_lexical = lexical_path(protected_path)
        protected_resolved = protected_path.expanduser().resolve()
        if paths_overlap(profile_lexical, protected_lexical):
            violations.append(
                f"  lexical overlap with {label}: {protected_lexical}"
            )
        elif paths_overlap(profile, protected_resolved):
            violations.append(
                f"  resolved overlap with {label}: {protected_resolved}"
            )

    if violations:
        raise SystemExit(
            "Refusing unsafe combined-emulator profile location "
            f"{profile_lexical} (resolved as {profile}):\n"
            + "\n".join(violations)
        )


def ensure_directory(path: Path) -> None:
    if path.is_symlink():
        raise SystemExit(f"Refusing symlinked profile directory: {path}")
    if path.exists() and not path.is_dir():
        raise SystemExit(f"Refusing non-directory profile path: {path}")
    path.mkdir(parents=True, exist_ok=True)


def replace_symlink(link: Path, target: Path) -> None:
    if link.is_symlink():
        link.unlink()
    elif link.exists():
        raise SystemExit(f"Refusing to replace non-symlink path: {link}")
    link.parent.mkdir(parents=True, exist_ok=True)
    link.symlink_to(os.path.relpath(target, link.parent))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def checked_directory_files(directory: Path) -> tuple[Path, ...]:
    """Return a deterministic flat fixture inventory, rejecting indirection."""

    files: list[Path] = []
    for entry in sorted(directory.iterdir()):
        if entry.is_symlink() or not entry.is_file():
            raise SystemExit(
                f"Fixture directories must contain only regular files: "
                f"{entry}"
            )
        files.append(entry)
    if not files:
        raise SystemExit(f"Fixture directory is empty: {directory}")
    return tuple(files)


def select_nominated_artifact(
    name: str, candidates: tuple[Path, ...]
) -> Path:
    expected = NOMINATED_WOLF_HASHES[name]
    observed = []
    for path in candidates:
        if not path.is_file():
            observed.append(f"  missing: {path}")
            continue
        actual = sha256(path)
        if actual == expected:
            return path
        observed.append(f"  {actual}: {path}")
    raise SystemExit(
        f"No nominated {name} artifact with SHA-256 {expected}:\n"
        + "\n".join(observed)
    )


def snapshot_file(source: Path, destination: Path) -> None:
    if destination.is_symlink():
        raise SystemExit(f"Refusing symlinked nominated snapshot: {destination}")
    if destination.exists() and not destination.is_file():
        raise SystemExit(f"Refusing non-file nominated snapshot: {destination}")
    if source == destination:
        return
    temporary = destination.with_name(f".{destination.name}.new")
    if temporary.exists() or temporary.is_symlink():
        raise SystemExit(f"Refusing existing temporary snapshot: {temporary}")
    shutil.copy2(source, temporary)
    os.replace(temporary, destination)


def git_identity(path: Path) -> tuple[Path, str, bool]:
    repository = Path(
        subprocess.run(
            ("git", "-C", str(path), "rev-parse", "--show-toplevel"),
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
    )
    commit = subprocess.run(
        ("git", "-C", str(repository), "rev-parse", "HEAD"),
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    dirty = bool(
        subprocess.run(
            ("git", "-C", str(repository), "status", "--porcelain"),
            check=True,
            capture_output=True,
            text=True,
        ).stdout
    )
    return repository, commit, dirty


def write_visual_autoexec(path: Path, refresh: bool) -> None:
    if path.is_symlink():
        raise SystemExit(f"Refusing symlinked autoexec: {path}")
    if path.exists() and not path.is_file():
        raise SystemExit(f"Refusing non-file autoexec: {path}")
    if not path.exists() or refresh:
        path.write_bytes(VISUAL_AUTOEXEC)


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Create or repair the isolated mutable combined-Pingo/Wolf Fab "
            "emulator profile. Existing autoexec.txt is preserved by default."
        )
    )
    parser.add_argument("--profile", type=Path, default=DEFAULT_PROFILE)
    parser.add_argument("--fab-root", type=Path, default=DEFAULT_FAB_ROOT)
    parser.add_argument("--module", type=Path, default=DEFAULT_MODULE)
    parser.add_argument(
        "--pingoasm-root", type=Path, default=DEFAULT_PINGOASM_ROOT
    )
    parser.add_argument("--wolf-root", type=Path, default=DEFAULT_WOLF_ROOT)
    parser.add_argument(
        "--ez80asm",
        default="ez80asm",
        help="assembler used for the integration-owned headless exit fixture",
    )
    parser.add_argument(
        "--refresh-autoexec",
        action="store_true",
        help="explicitly restore the documented two-stage visual gate",
    )
    args = parser.parse_args()

    profile = args.profile.expanduser().resolve()
    fab_root = args.fab_root.expanduser().resolve()
    module = args.module.expanduser().resolve()
    pingoasm_root = args.pingoasm_root.expanduser().resolve()
    wolf_root = args.wolf_root.expanduser().resolve()

    # This is deliberately the first validation after argument normalization.
    # A rejected profile must not create, remove, or relink anything.
    validate_profile_location(
        args.profile,
        profile,
        fab_root=fab_root,
        pingoasm_root=pingoasm_root,
        wolf_root=wolf_root,
    )

    fab_executable = fab_root / "target/release/fab-agon-emulator"
    mos = fab_root / "firmware/mos_console8.bin"
    mos_map = mos.with_suffix(".map")
    shared_sd = fab_root / "sdcard"
    pingo_apps = pingoasm_root / "apps"
    pingo_visual = (
        pingo_apps / "earth-party-flat-local/tgt/earth-party-flat.bin"
    )
    pingo_benchmarks = pingoasm_root / "benchmarks/render-spin/fixtures"
    pingo_smoke_dir = pingo_benchmarks / "cube-rgba2222/tgt"
    pingo_smoke = pingo_smoke_dir / "benchmark.bin"
    canonical_wolf_binary = wolf_root / "src/asm/wolf3d/tgt/wolf3d.bin"
    accepted_wolf_binary = (
        wolf_root / "agonport/emulator/sdcard/wolf3d/wolf3d.bin"
    )
    combined_smoke_source = SOURCE_ROOT / "userspace/combined_emulator_exit.asm"
    canonical_wolf_assets = {
        "tiles.agnb": (
            wolf_root
            / "agonport/assets/generated/wolf3d/levels/level_00/tiles.agnb"
        ),
        "sprites.agnb": (
            wolf_root
            / "agonport/assets/generated/wolf3d/levels/level_00/sprites.agnb"
        ),
        "hud.agnb": (
            wolf_root / "agonport/assets/generated/wolf3d/hud/hud.agnb"
        ),
        "sfx.agnb": (
            wolf_root / "agonport/assets/generated/wolf3d/audio/sfx.agnb"
        ),
    }

    required = (
        fab_executable,
        mos,
        mos_map,
        module,
        shared_sd / "bin",
        shared_sd / "mos",
        shared_sd / "firmware.bin",
        shared_sd / "MOS.bin",
        pingo_apps,
        pingo_visual,
        pingo_smoke,
        pingo_smoke_dir / "blenderaxes.rgba2",
        combined_smoke_source,
    )
    missing = [path for path in required if not path.exists()]
    if missing:
        raise SystemExit(
            "Required combined-emulator paths are missing:\n"
            + "\n".join(f"  {path}" for path in missing)
        )
    pingo_visual_files = checked_directory_files(pingo_visual.parent)
    pingo_smoke_files = checked_directory_files(pingo_smoke_dir)

    ensure_directory(profile)
    sdcard = profile / "sdcard"
    wolf_snapshot_dir = profile / "snapshots/wolf3d"
    ensure_directory(sdcard)
    ensure_directory(sdcard / "mystuff/pingoasm")
    ensure_directory(sdcard / "wolf3d")
    ensure_directory(sdcard / "combined-smoke")
    ensure_directory(wolf_snapshot_dir)

    wolf_snapshot_paths = {
        name: wolf_snapshot_dir / name for name in NOMINATED_WOLF_HASHES
    }
    accepted_combined_snapshots = DEFAULT_PROFILE / "snapshots/wolf3d"
    wolf_sources = {
        "wolf3d.bin": select_nominated_artifact(
            "wolf3d.bin",
            (
                canonical_wolf_binary,
                accepted_wolf_binary,
                accepted_combined_snapshots / "wolf3d.bin",
                wolf_snapshot_paths["wolf3d.bin"],
            ),
        )
    }
    for name, canonical in canonical_wolf_assets.items():
        wolf_sources[name] = select_nominated_artifact(
            name,
            (
                canonical,
                wolf_root / "agonport/emulator/sdcard/wolf3d" / name,
                accepted_combined_snapshots / name,
                wolf_snapshot_paths[name],
            ),
        )
    for name, source in wolf_sources.items():
        snapshot_file(source, wolf_snapshot_paths[name])
        actual = sha256(wolf_snapshot_paths[name])
        if actual != NOMINATED_WOLF_HASHES[name]:
            raise SystemExit(
                f"Nominated Wolf snapshot verification failed: {name}"
            )

    replace_symlink(profile / "fab-agon-emulator", fab_executable)
    replace_symlink(profile / "mos_console8.bin", mos)
    replace_symlink(profile / "vdp_combined.so", module)

    replace_symlink(sdcard / "bin", shared_sd / "bin")
    replace_symlink(sdcard / "mos", shared_sd / "mos")
    replace_symlink(sdcard / "firmware.bin", shared_sd / "firmware.bin")
    replace_symlink(sdcard / "MOS.bin", shared_sd / "MOS.bin")
    replace_symlink(sdcard / "mystuff/pingoasm/apps", pingo_apps)
    replace_symlink(sdcard / "pingo", pingo_benchmarks)

    for name, target in wolf_snapshot_paths.items():
        replace_symlink(sdcard / "wolf3d" / name, target)
    obsolete_wolf_hello = sdcard / "wolf3d_hello"
    if obsolete_wolf_hello.is_symlink():
        obsolete_wolf_hello.unlink()
    combined_smoke = sdcard / "combined-smoke/wolf-dispatch-exit.bin"
    if combined_smoke.is_symlink():
        raise SystemExit(
            f"Refusing symlinked combined smoke artifact: {combined_smoke}"
        )
    temporary_smoke = combined_smoke.with_name(
        f".{combined_smoke.name}.new"
    )
    if temporary_smoke.exists() or temporary_smoke.is_symlink():
        raise SystemExit(
            f"Refusing existing temporary smoke artifact: {temporary_smoke}"
        )
    with tempfile.TemporaryDirectory(prefix="wolf-pingo-smoke.") as build:
        build_dir = Path(build)
        local_source = build_dir / "exit.asm"
        local_binary = build_dir / "exit.bin"
        shutil.copy2(combined_smoke_source, local_source)
        subprocess.run(
            (args.ez80asm, local_source.name, local_binary.name),
            check=True,
            cwd=build_dir,
        )
        shutil.copy2(local_binary, temporary_smoke)
    os.replace(temporary_smoke, combined_smoke)

    autoexec = sdcard / "autoexec.txt"
    write_visual_autoexec(autoexec, args.refresh_autoexec)

    source_repository, source_commit, source_dirty = git_identity(SOURCE_ROOT)
    fab_repository, fab_commit, fab_dirty = git_identity(fab_root)
    manifest_lines = [
        "profile=wolf-pingo-v2.16",
        "kind=mutable-development",
        f"combined_source_repository={source_repository}",
        f"combined_source_commit={source_commit}",
        f"combined_source_dirty={'yes' if source_dirty else 'no'}",
        f"combined_module={module}",
        f"combined_module_sha256={sha256(module)}",
        f"fab_repository={fab_repository}",
        f"fab_commit={fab_commit}",
        f"fab_dirty={'yes' if fab_dirty else 'no'}",
        f"fab_executable_sha256={sha256(fab_executable)}",
        f"mos_sha256={sha256(mos)}",
        f"mos_map_sha256={sha256(mos_map)}",
        f"pingo_visual={pingo_visual}",
        f"pingo_visual_sha256={sha256(pingo_visual)}",
        f"pingo_smoke={pingo_smoke}",
        f"pingo_smoke_sha256={sha256(pingo_smoke)}",
        f"wolf_visual_source={wolf_sources['wolf3d.bin']}",
        f"wolf_visual_snapshot={wolf_snapshot_paths['wolf3d.bin']}",
        f"wolf_visual_sha256={sha256(wolf_snapshot_paths['wolf3d.bin'])}",
    ]
    for name in ("tiles.agnb", "sprites.agnb", "hud.agnb", "sfx.agnb"):
        manifest_lines.extend(
            (
                f"wolf_{name}_source={wolf_sources[name]}",
                f"wolf_{name}_snapshot={wolf_snapshot_paths[name]}",
                f"wolf_{name}_sha256={sha256(wolf_snapshot_paths[name])}",
            )
        )
    manifest_lines.extend(
        (
            f"wolf_smoke_source={combined_smoke_source}",
            f"wolf_smoke_source_sha256={sha256(combined_smoke_source)}",
            f"wolf_smoke_binary={combined_smoke}",
            f"wolf_smoke_binary_sha256={sha256(combined_smoke)}",
        )
    )
    (profile / "profile-manifest.txt").write_text(
        "\n".join(manifest_lines) + "\n", encoding="utf-8"
    )

    checked_artifacts = (
        module,
        fab_executable,
        mos,
        mos_map,
        *pingo_visual_files,
        *pingo_smoke_files,
        combined_smoke_source,
        combined_smoke,
        *wolf_snapshot_paths.values(),
    )
    checksums = "".join(
        f"{sha256(path)}  {path}\n" for path in checked_artifacts
    )
    (profile / "profile-checksums.sha256").write_text(
        checksums, encoding="utf-8"
    )

    print(f"Combined emulator profile: {profile}")
    print(f"Combined VDP:              {profile / 'vdp_combined.so'}")
    print(f"Autoexec:                  {autoexec}")
    if args.refresh_autoexec:
        print("Visual autoexec:            explicitly refreshed")
    else:
        print("Visual autoexec:            preserved if already present")


if __name__ == "__main__":
    main()
