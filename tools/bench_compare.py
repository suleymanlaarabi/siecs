#!/usr/bin/env python3
"""Compare local benchmarks with the current origin/main benchmark suite."""

from __future__ import annotations

import argparse
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path


BENCH_LINE = re.compile(r"^\[bench\] ([^:]+): ([0-9]+(?:\.[0-9]+)?) ms$")


def run(command: list[str], cwd: Path, *, capture: bool = False) -> str:
    result = subprocess.run(
        command,
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.STDOUT if capture else None,
        check=False,
    )
    if result.returncode != 0:
        output = result.stdout or ""
        raise RuntimeError(f"command failed ({result.returncode}): {' '.join(command)}\n{output}")
    return result.stdout or ""


def build_benchmarks(cwd: Path) -> None:
    run(["bake", "rebuild", "bench", "-r", "--cfg", "release"], cwd, capture=True)


def run_benchmarks(cwd: Path, scope: str | None, cpu: int | None) -> dict[str, list[float]]:
    command = ["bake", "run", "bench", "--cfg", "release"]
    if scope:
        command += ["--", scope]
    if cpu is not None:
        if shutil.which("taskset") is None:
            raise RuntimeError("--cpu requires taskset on this platform")
        command = ["taskset", "-c", str(cpu), *command]

    output = run(command, cwd, capture=True)
    measurements: dict[str, list[float]] = {}
    for line in output.splitlines():
        match = BENCH_LINE.match(line.strip())
        if match:
            name = match.group(1)
            if not scope or name == scope or name.startswith(scope + "_"):
                measurements.setdefault(name, []).append(float(match.group(2)))

    if not measurements:
        raise RuntimeError("benchmark command produced no parsable '[bench]' results")
    return measurements


def median_measurements(measurements: dict[str, list[float]]) -> dict[str, float]:
    return {name: statistics.median(values) for name, values in measurements.items()}


def aggregate_runs(runs: list[dict[str, list[float]]]) -> dict[str, float]:
    measurements: dict[str, list[float]] = {}
    for run_measurements in runs:
        for name, values in run_measurements.items():
            measurements.setdefault(name, []).extend(values)
    return median_measurements(measurements)


def format_number(value: float | None) -> str:
    return "-" if value is None else f"{value:.3f}"


def compare(
    online: dict[str, float], local: dict[str, float], threshold: float
) -> list[tuple[str, float | None, float | None, float | None, float | None, str]]:
    rows = []
    for name in sorted(set(online) | set(local)):
        old = online.get(name)
        new = local.get(name)
        if old is None:
            rows.append((name, None, new, None, None, "NEW"))
            continue
        if new is None:
            rows.append((name, old, None, None, None, "REMOVED"))
            continue

        delta = new - old
        percent = 0.0 if old == 0 else delta / old * 100.0
        if percent < -threshold:
            result = "GAIN"
        elif percent > threshold:
            result = "REGRESSION"
        else:
            result = "STABLE"
        rows.append((name, old, new, delta, percent, result))

    return rows


def print_report(rows, runs: int, threshold: float, scope: str | None) -> None:
    title = "Benchmark comparison"
    if scope:
        title += f" (scope: {scope})"
    print(f"\n{title}: origin/main vs local")
    print(f"Median of {runs} run(s), stability threshold: ±{threshold:.1f}%")
    headers = ("Benchmark", "Online ms", "Local ms", "Delta ms", "Delta %", "Result")
    widths = [max(len(headers[0]), *(len(row[0]) for row in rows))]
    widths += [10, 9, 9, 9, 10]

    def line(values):
        print("  ".join(value.ljust(width) for value, width in zip(values, widths)))

    line(headers)
    line(tuple("-" * width for width in widths))
    for name, old, new, delta, percent, result in rows:
        line(
            (
                name,
                format_number(old),
                format_number(new),
                format_number(delta),
                "-" if percent is None else f"{percent:+.2f}%",
                result,
            )
        )

    counts = {result: sum(row[-1] == result for row in rows) for result in
              ("GAIN", "REGRESSION", "STABLE", "NEW", "REMOVED")}
    print(
        "\nSummary: "
        + ", ".join(f"{name.lower()}={count}" for name, count in counts.items())
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scope", help="only compare one benchmark scope")
    parser.add_argument("--runs", type=int, default=10, help="number of runs (default: 10)")
    parser.add_argument(
        "--cpu",
        type=int,
        default=0,
        help="pin benchmark processes to this CPU (default: 0)",
    )
    parser.add_argument(
        "--threshold", type=float, default=5.0, help="stable threshold in percent (default: 5)"
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.runs < 1 or args.threshold < 0:
        print("--runs must be positive and --threshold must not be negative", file=sys.stderr)
        return 2

    root = Path(__file__).resolve().parents[1]
    try:
        run(["git", "fetch", "origin", "main"], root, capture=True)
        online_ref = run(["git", "rev-parse", "origin/main"], root, capture=True).strip()

        with tempfile.TemporaryDirectory(prefix="siecs-bench-compare-") as temporary:
            baseline = Path(temporary) / "baseline"
            run(["git", "worktree", "add", "--detach", str(baseline), online_ref], root, capture=True)
            try:
                shutil.copy2(root / "bench/src/main.c", baseline / "bench/src/main.c")
                print(f"Building origin/main ({online_ref[:12]})...", file=sys.stderr)
                build_benchmarks(baseline)
                online_runs = [run_benchmarks(baseline, args.scope, args.cpu) for _ in range(args.runs)]

                print("Building local benchmark...", file=sys.stderr)
                build_benchmarks(root)
                local_runs = [run_benchmarks(root, args.scope, args.cpu) for _ in range(args.runs)]
            finally:
                run(["git", "worktree", "remove", "--force", str(baseline)], root, capture=True)

        online_values = aggregate_runs(online_runs)
        local_values = aggregate_runs(local_runs)
        print_report(compare(online_values, local_values, args.threshold), args.runs, args.threshold, args.scope)
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"bench-compare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
