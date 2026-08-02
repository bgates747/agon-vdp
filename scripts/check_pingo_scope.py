#!/usr/bin/env python3
"""Verify TurboVega's Pingo surface plus qualified local extensions."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
BRIDGE = ROOT / "video/pingo_3d.h"
BUFFERED = ROOT / "video/vdu_buffered.h"

# Commands 39 and 42 are intentionally not inner dispatch entries: 39 is the
# outer control-lifecycle route and 42 remains unassigned.  Keep experimental
# terrain commands explicit here so a new handler cannot silently widen the
# audited protocol surface.
EXPECTED_DISPATCH = set(range(1, 39)) | {40, 41} | set(range(43, 54))


def main() -> None:
    bridge = BRIDGE.read_text()
    buffered = BUFFERED.read_text()

    actual = {int(value) for value in re.findall(r"\bcase\s+(\d+)\s*:", bridge)}
    if actual != EXPECTED_DISPATCH:
        missing = sorted(EXPECTED_DISPATCH - actual)
        unexpected = sorted(actual - EXPECTED_DISPATCH)
        raise SystemExit(
            f"Pingo dispatch mismatch; missing={missing}, unexpected={unexpected}"
        )

    required_outer_routes = (0, 39)
    absent = [
        route
        for route in required_outer_routes
        if not re.search(rf"\bsub(?:command|cmd)\s*==\s*{route}\b", buffered)
    ]
    if absent:
        raise SystemExit(f"Missing outer lifecycle routes: {absent}")

    print(
        "Pingo scope verified: TurboVega 0-40 plus local extensions 41, 43-53"
    )


if __name__ == "__main__":
    main()
