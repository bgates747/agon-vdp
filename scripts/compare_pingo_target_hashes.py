#!/usr/bin/env python3
"""Validate and compare emulator-only Pingo render-target hash streams."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import sys


TARGET_RE = re.compile(
    r"PINGO_TARGET "
    r"seq=(?P<seq>\d+) "
    r"bmid=(?P<bmid>\d+) "
    r"bytes=(?P<byte_count>\d+) "
    r"fnv1a64=(?P<hash>[0-9a-fA-F]{16}) "
    r"zbytes=(?P<zeta_byte_count>\d+) "
    r"zfnv1a64=(?P<zeta_hash>[0-9a-fA-F]{16})"
)


@dataclass(frozen=True)
class TargetHash:
    sequence: int
    bitmap_id: int
    byte_count: int
    fnv1a64: str
    zeta_byte_count: int
    zeta_fnv1a64: str

    @property
    def key(self) -> tuple[int, int]:
        return self.bitmap_id, self.sequence

    def canonical_line(self) -> str:
        return (
            f"PINGO_TARGET seq={self.sequence} bmid={self.bitmap_id} "
            f"bytes={self.byte_count} fnv1a64={self.fnv1a64} "
            f"zbytes={self.zeta_byte_count} "
            f"zfnv1a64={self.zeta_fnv1a64}"
        )


def parse_log(path: Path) -> list[TargetHash]:
    records: list[TargetHash] = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = TARGET_RE.search(line)
        if match is None:
            continue
        records.append(
            TargetHash(
                sequence=int(match.group("seq")),
                bitmap_id=int(match.group("bmid")),
                byte_count=int(match.group("byte_count")),
                fnv1a64=match.group("hash").lower(),
                zeta_byte_count=int(match.group("zeta_byte_count")),
                zeta_fnv1a64=match.group("zeta_hash").lower(),
            )
        )
    if not records:
        raise ValueError(f"{path}: no PINGO_TARGET records")
    return records


def parse_expected_stream(value: str) -> tuple[int, int]:
    try:
        bitmap_id_text, count_text = value.split(":", 1)
        bitmap_id = int(bitmap_id_text)
        count = int(count_text)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            f"{value!r} is not BMID:COUNT"
        ) from exc
    if bitmap_id < 0 or count <= 0:
        raise argparse.ArgumentTypeError(
            f"{value!r} requires BMID >= 0 and COUNT > 0"
        )
    return bitmap_id, count


def validate_records(
    path: Path,
    records: list[TargetHash],
    expected_streams: list[tuple[int, int]],
) -> list[str]:
    errors: list[str] = []
    seen: set[tuple[int, int]] = set()
    streams: dict[int, list[int]] = {}

    for record in records:
        if record.key in seen:
            errors.append(
                f"{path}: duplicate bmid={record.bitmap_id} "
                f"seq={record.sequence}"
            )
        seen.add(record.key)
        streams.setdefault(record.bitmap_id, []).append(record.sequence)

    for bitmap_id, sequences in sorted(streams.items()):
        expected_sequences = list(range(len(sequences)))
        if sequences != expected_sequences:
            errors.append(
                f"{path}: bmid={bitmap_id} sequences are not contiguous "
                f"0..{len(sequences) - 1}"
            )

    if expected_streams:
        expected = dict(expected_streams)
        if len(expected) != len(expected_streams):
            errors.append("duplicate --expected-stream bitmap ID")
        actual = {bitmap_id: len(sequences)
                  for bitmap_id, sequences in streams.items()}
        if actual != expected:
            errors.append(
                f"{path}: stream counts {actual!r}, expected {expected!r}"
            )

    return errors


def compare_records(
    baseline_path: Path,
    baseline: list[TargetHash],
    candidate_path: Path,
    candidate: list[TargetHash],
) -> list[str]:
    errors: list[str] = []
    if len(baseline) != len(candidate):
        errors.append(
            f"record count differs: {baseline_path}={len(baseline)}, "
            f"{candidate_path}={len(candidate)}"
        )

    for index, (expected, actual) in enumerate(zip(baseline, candidate)):
        if expected != actual:
            errors.append(
                f"record {index} differs:\n"
                f"  expected {expected.canonical_line()}\n"
                f"  actual   {actual.canonical_line()}"
            )
            if len(errors) >= 20:
                errors.append("additional differences suppressed")
                break
    return errors


def describe(records: list[TargetHash]) -> str:
    counts: dict[int, int] = {}
    for record in records:
        counts[record.bitmap_id] = counts.get(record.bitmap_id, 0) + 1
    streams = ", ".join(
        f"bmid {bitmap_id}: {count}"
        for bitmap_id, count in sorted(counts.items())
    )
    return f"{len(records)} records ({streams})"


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Validate PINGO_TARGET records and optionally require a candidate "
            "run to match a baseline pixel-for-pixel."
        )
    )
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", nargs="?", type=Path)
    parser.add_argument(
        "--expected-stream",
        action="append",
        default=[],
        metavar="BMID:COUNT",
        type=parse_expected_stream,
    )
    parser.add_argument(
        "--extract",
        type=Path,
        metavar="PATH",
        help="write the baseline records in compact canonical form",
    )
    args = parser.parse_args()

    try:
        baseline = parse_log(args.baseline)
        errors = validate_records(
            args.baseline, baseline, args.expected_stream
        )

        if args.candidate is not None:
            candidate = parse_log(args.candidate)
            errors.extend(
                validate_records(
                    args.candidate, candidate, args.expected_stream
                )
            )
            errors.extend(
                compare_records(
                    args.baseline, baseline, args.candidate, candidate
                )
            )
    except (OSError, ValueError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1

    if errors:
        for error in errors:
            print(f"FAIL: {error}", file=sys.stderr)
        return 1

    if args.extract is not None:
        args.extract.write_text(
            "".join(record.canonical_line() + "\n"
                    for record in baseline),
            encoding="utf-8",
        )

    if args.candidate is None:
        print(f"PASS: {args.baseline}: {describe(baseline)}")
    else:
        print(
            "PASS: exact render-target match: "
            f"{describe(baseline)}"
        )
    if args.extract is not None:
        print(f"Wrote {args.extract}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
