#!/usr/bin/env python3
"""Verify that the clean port exposes only TurboVega's Pingo subcommands."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
BRIDGE = ROOT / "video/pingo_3d.h"
BUFFERED = ROOT / "video/vdu_buffered.h"

EXPECTED_DISPATCH = set(range(1, 39)) | {40}


def main() -> None:
    bridge = BRIDGE.read_text()
    buffered = BUFFERED.read_text()

    actual = {int(value) for value in re.findall(r"\bcase\s+(\d+)\s*:", bridge)}
    if actual != EXPECTED_DISPATCH:
        missing = sorted(EXPECTED_DISPATCH - actual)
        unexpected = sorted(actual - EXPECTED_DISPATCH)
        raise SystemExit(
            f"TurboVega dispatch mismatch; missing={missing}, unexpected={unexpected}"
        )

    required_outer_routes = (0, 39)
    absent = [
        route
        for route in required_outer_routes
        if not re.search(rf"\bsub(?:command|cmd)\s*==\s*{route}\b", buffered)
    ]
    if absent:
        raise SystemExit(f"Missing outer lifecycle routes: {absent}")

    print("TurboVega Pingo scope verified: 0-40, with no later dispatch commands")


if __name__ == "__main__":
    main()
