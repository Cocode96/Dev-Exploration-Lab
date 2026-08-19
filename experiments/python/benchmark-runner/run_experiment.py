"""Repeat command-line experiments and create CSV/Markdown results.

The target program may print one line in this form to report its own metrics:

    EXPERIMENT_RESULT: {"switch_ms": 12.4, "debug_errors": 0}

Only Python's standard library is used so that the runner is easy to inspect.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import random
import shlex
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any


RESULT_PREFIX = "EXPERIMENT_RESULT:"


@dataclass(frozen=True)
class Variant:
    """One implementation to execute, such as single_dll or swap_dll."""

    name: str
    command: list[str]


@dataclass(frozen=True)
class ExperimentConfig:
    """Validated settings loaded from the JSON configuration file."""

    name: str
    iterations: int
    warmup: int
    timeout_seconds: float
    random_seed: int
    output_directory: Path
    working_directory: Path
    variants: list[Variant]


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run command variants repeatedly and summarize their results."
    )
    parser.add_argument(
        "config",
        nargs="?",
        type=Path,
        help="Path to an experiment JSON file. Omit it for interactive setup.",
    )
    return parser.parse_args()


def ask(prompt: str, default: str) -> str:
    """Show a default value so interactive setup remains quick to complete."""

    answer = input(f"{prompt} [{default}]: ").strip()
    return answer or default


def ask_command(variant_name: str) -> list[str]:
    """Build a command from an executable path and optional command-line arguments."""

    while True:
        executable_text = input(f"{variant_name} EXE path: ").strip().strip('"')
        executable = Path(executable_text).expanduser()
        if executable.is_file():
            break
        print(f"File not found: {executable}")

    arguments_text = input(f"{variant_name} arguments (optional): ").strip()
    arguments = shlex.split(arguments_text, posix=False) if arguments_text else []
    return [str(executable.resolve()), *arguments]


def create_interactive_config() -> ExperimentConfig:
    """Ask only the questions needed for a two-variant comparison."""

    print("No configuration file was given. Creating an experiment interactively.")
    name = ask("Experiment name", "exe-comparison")

    first_name = ask("First variant name", "variant_a")
    first_command = ask_command(first_name)
    second_name = ask("Second variant name", "variant_b")
    second_command = ask_command(second_name)

    config = ExperimentConfig(
        name=name,
        iterations=int(ask("Iterations per variant", "30")),
        warmup=int(ask("Warm-up runs per variant", "3")),
        timeout_seconds=float(ask("Timeout in seconds", "30")),
        random_seed=int(ask("Random seed", "20260819")),
        output_directory=Path.cwd() / "results",
        working_directory=Path.cwd(),
        variants=[
            Variant(first_name, first_command),
            Variant(second_name, second_command),
        ],
    )
    validate_config(config)
    return config


def load_config(config_path: Path) -> ExperimentConfig:
    """Read JSON and convert it into a small, predictable configuration object."""

    with config_path.open("r", encoding="utf-8") as file:
        raw = json.load(file)

    variants = [
        Variant(
            name=item["name"],
            command=[
                sys.executable if part == "{python}" else part
                for part in item["command"]
            ],
        )
        for item in raw["variants"]
    ]

    config = ExperimentConfig(
        name=raw["name"],
        iterations=int(raw.get("iterations", 10)),
        warmup=int(raw.get("warmup", 2)),
        timeout_seconds=float(raw.get("timeout_seconds", 30)),
        random_seed=int(raw.get("random_seed", 20260819)),
        output_directory=(config_path.parent / raw.get("output_directory", "results")),
        working_directory=Path(raw.get("working_directory", config_path.parent)),
        variants=variants,
    )
    validate_config(config)
    return config


def validate_config(config: ExperimentConfig) -> None:
    """Reject common configuration mistakes before any program is executed."""

    if not config.name.strip():
        raise ValueError("name must not be empty")
    if config.iterations < 1:
        raise ValueError("iterations must be at least 1")
    if config.warmup < 0:
        raise ValueError("warmup must be 0 or greater")
    if config.timeout_seconds <= 0:
        raise ValueError("timeout_seconds must be greater than 0")
    if len(config.variants) < 2:
        raise ValueError("at least two variants are required for comparison")

    names = [variant.name for variant in config.variants]
    if len(names) != len(set(names)):
        raise ValueError("variant names must be unique")

    for variant in config.variants:
        if not variant.command or not all(isinstance(part, str) for part in variant.command):
            raise ValueError(f"{variant.name}: command must be a non-empty string list")


def extract_target_metrics(stdout: str) -> dict[str, int | float]:
    """Read numeric metrics from the last EXPERIMENT_RESULT line."""

    for line in reversed(stdout.splitlines()):
        if not line.startswith(RESULT_PREFIX):
            continue

        value = json.loads(line.removeprefix(RESULT_PREFIX).strip())
        if not isinstance(value, dict):
            raise ValueError("EXPERIMENT_RESULT must contain a JSON object")

        return {
            key: metric
            for key, metric in value.items()
            if isinstance(metric, (int, float)) and not isinstance(metric, bool)
        }

    return {}


def run_once(
    variant: Variant,
    run_number: int,
    timeout_seconds: float,
    working_directory: Path,
) -> dict[str, Any]:
    """Execute one process and return a flat row suitable for a CSV file."""

    started_ns = time.perf_counter_ns()
    try:
        completed = subprocess.run(
            variant.command,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout_seconds,
            cwd=working_directory,
            check=False,
        )
        elapsed_ms = (time.perf_counter_ns() - started_ns) / 1_000_000

        row: dict[str, Any] = {
            "variant": variant.name,
            "run": run_number,
            "success": completed.returncode == 0,
            "timed_out": False,
            "return_code": completed.returncode,
            "process_ms": round(elapsed_ms, 6),
            "error": completed.stderr.strip(),
        }

        try:
            row.update(extract_target_metrics(completed.stdout))
        except (json.JSONDecodeError, ValueError) as error:
            row["success"] = False
            row["error"] = f"Invalid target metrics: {error}"

        return row

    except subprocess.TimeoutExpired:
        elapsed_ms = (time.perf_counter_ns() - started_ns) / 1_000_000
        return {
            "variant": variant.name,
            "run": run_number,
            "success": False,
            "timed_out": True,
            "return_code": "",
            "process_ms": round(elapsed_ms, 6),
            "error": f"Timed out after {timeout_seconds} seconds",
        }


def make_run_order(config: ExperimentConfig) -> list[tuple[Variant, int]]:
    """Mix A/B execution order to reduce bias caused by always running A first."""

    jobs = [
        (variant, run_number)
        for run_number in range(1, config.iterations + 1)
        for variant in config.variants
    ]
    random.Random(config.random_seed).shuffle(jobs)
    return jobs


def percentile(values: list[float], percent: float) -> float:
    """Return a nearest-rank percentile without requiring NumPy."""

    ordered = sorted(values)
    index = max(0, math.ceil(percent / 100 * len(ordered)) - 1)
    return ordered[index]


def numeric_metric_names(rows: list[dict[str, Any]]) -> list[str]:
    ignored = {"run", "success", "timed_out", "return_code"}
    names = {
        key
        for row in rows
        for key, value in row.items()
        if key not in ignored
        and isinstance(value, (int, float))
        and not isinstance(value, bool)
    }
    return sorted(names)


def build_summary(config: ExperimentConfig, rows: list[dict[str, Any]]) -> str:
    """Create a human-readable report from successful measurements."""

    lines = [
        f"# {config.name}",
        "",
        f"Generated: {datetime.now().astimezone().isoformat(timespec='seconds')}",
        "",
        f"Iterations per variant: {config.iterations}, warm-ups: {config.warmup}",
        "",
        "| Variant | Success | Metric | Median | P95 | Min | Max |",
        "|---|---:|---|---:|---:|---:|---:|",
    ]

    metric_names = numeric_metric_names(rows)
    for variant in config.variants:
        variant_rows = [row for row in rows if row["variant"] == variant.name]
        successful_rows = [row for row in variant_rows if row["success"]]
        success_text = f"{len(successful_rows)}/{len(variant_rows)}"

        if not successful_rows:
            lines.append(f"| {variant.name} | {success_text} | - | - | - | - | - |")
            continue

        for metric_name in metric_names:
            values = [
                float(row[metric_name])
                for row in successful_rows
                if metric_name in row
            ]
            if not values:
                continue
            lines.append(
                f"| {variant.name} | {success_text} | {metric_name} "
                f"| {statistics.median(values):.3f} | {percentile(values, 95):.3f} "
                f"| {min(values):.3f} | {max(values):.3f} |"
            )

    lines.extend(
        [
            "",
            "## Notes",
            "",
            "- Warm-up runs are not included in this report.",
            "- Process time includes process startup and shutdown.",
            "- Target metrics come from the program's EXPERIMENT_RESULT output.",
            "- A successful run means exit code 0, no timeout, and valid target metrics.",
            "",
        ]
    )
    return "\n".join(lines)


def write_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    field_names = sorted({key for row in rows for key in row})
    preferred = [
        "variant",
        "run",
        "success",
        "timed_out",
        "return_code",
        "process_ms",
        "error",
    ]
    ordered_fields = [name for name in preferred if name in field_names]
    ordered_fields += [name for name in field_names if name not in ordered_fields]

    with path.open("w", newline="", encoding="utf-8-sig") as file:
        writer = csv.DictWriter(file, fieldnames=ordered_fields)
        writer.writeheader()
        writer.writerows(rows)


def write_config_snapshot(path: Path, config: ExperimentConfig) -> None:
    """Save the exact inputs needed to repeat an interactive experiment."""

    data = {
        "name": config.name,
        "iterations": config.iterations,
        "warmup": config.warmup,
        "timeout_seconds": config.timeout_seconds,
        "random_seed": config.random_seed,
        "output_directory": str(config.output_directory),
        "working_directory": str(config.working_directory),
        "variants": [
            {"name": variant.name, "command": variant.command}
            for variant in config.variants
        ],
    }
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")


def run_experiment(config: ExperimentConfig) -> tuple[Path, Path]:
    """Run warm-ups, collect measurements, and save both output files."""

    print(f"Experiment: {config.name}")
    print("Warm-up runs are starting...")
    for variant in config.variants:
        for warmup_number in range(1, config.warmup + 1):
            run_once(
                variant,
                warmup_number,
                config.timeout_seconds,
                config.working_directory,
            )

    jobs = make_run_order(config)
    rows: list[dict[str, Any]] = []
    for job_number, (variant, run_number) in enumerate(jobs, start=1):
        print(f"[{job_number}/{len(jobs)}] {variant.name}, run {run_number}")
        rows.append(
            run_once(
                variant,
                run_number,
                config.timeout_seconds,
                config.working_directory,
            )
        )

    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    output_directory = config.output_directory / f"{config.name}-{timestamp}"
    output_directory.mkdir(parents=True, exist_ok=False)

    csv_path = output_directory / "raw_results.csv"
    report_path = output_directory / "report.md"
    config_path = output_directory / "experiment_config.json"
    write_csv(csv_path, rows)
    report_path.write_text(build_summary(config, rows), encoding="utf-8")
    write_config_snapshot(config_path, config)
    return csv_path, report_path


def main() -> int:
    try:
        arguments = parse_arguments()
        config = (
            load_config(arguments.config.resolve())
            if arguments.config
            else create_interactive_config()
        )
        csv_path, report_path = run_experiment(config)
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1

    print(f"CSV: {csv_path}")
    print(f"Report: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
