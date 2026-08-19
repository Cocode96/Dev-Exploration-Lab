"""Convert renderer_compare.exe text output into Experiment Runner metrics.

This adapter does not change the C++ executable. It runs the existing program,
selects either its "separate" or "combined" result, and prints JSON using the
protocol understood by run_experiment.py.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


RESULT_PATTERN = re.compile(
    r"^(separate|combined)\s+"
    r"files=(\d+)\s+"
    r"load_us=([0-9.]+)\s+"
    r"total_switch_us=([0-9.]+)$",
    re.MULTILINE,
)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--mode", required=True, choices=("separate", "combined"))
    return parser.parse_args()


def read_results(stdout: str) -> dict[str, dict[str, int | float]]:
    """Turn the two result lines into a dictionary indexed by architecture."""

    results: dict[str, dict[str, int | float]] = {}
    for match in RESULT_PATTERN.finditer(stdout):
        mode, file_bytes, load_us, total_switch_us = match.groups()
        results[mode] = {
            "dll_bytes": int(file_bytes),
            "load_us": float(load_us),
            "total_switch_us": float(total_switch_us),
        }
    return results


def main() -> int:
    arguments = parse_arguments()
    executable = arguments.executable.resolve()
    if not executable.is_file():
        print(f"Executable not found: {executable}", file=sys.stderr)
        return 1

    completed = subprocess.run(
        [str(executable)],
        cwd=executable.parent,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if completed.returncode != 0:
        print(completed.stderr, file=sys.stderr)
        return completed.returncode

    results = read_results(completed.stdout)
    if arguments.mode not in results:
        print(f"Could not find the {arguments.mode} result in program output", file=sys.stderr)
        return 1

    print(f"EXPERIMENT_RESULT: {json.dumps(results[arguments.mode])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
