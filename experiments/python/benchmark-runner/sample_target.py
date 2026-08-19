"""Small target program used to verify the experiment runner."""

from __future__ import annotations

import argparse
import json
import time


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--name", required=True)
    parser.add_argument("--delay-ms", required=True, type=float)
    args = parser.parse_args()

    started_ns = time.perf_counter_ns()
    time.sleep(args.delay_ms / 1_000)
    measured_ms = (time.perf_counter_ns() - started_ns) / 1_000_000

    metrics = {
        "work_ms": round(measured_ms, 6),
        "debug_errors": 0,
    }
    print(f"{args.name} completed")
    print(f"EXPERIMENT_RESULT: {json.dumps(metrics)}")


if __name__ == "__main__":
    main()
