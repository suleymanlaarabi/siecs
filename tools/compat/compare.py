#!/usr/bin/env python3
"""Compare isolated oracle/candidate processes. Gaps never count as passing tests."""

import argparse
import difflib
import json
import subprocess
import sys
from pathlib import Path


def run(command, request):
    result = subprocess.run([str(command)], input=json.dumps(request), text=True,
                            capture_output=True, timeout=40)
    try:
        output = json.loads(result.stdout)
    except ValueError as error:
        raise ValueError(f"{command}: invalid output, exit {result.returncode}: "
                         f"{result.stderr[-2000:]}") from error
    return result.returncode, output


def comparable(output):
    return {key: output[key] for key in ("success", "entities", "diagnostics", "events")}


def compare(oracle, candidate):
    for code, output in (oracle, candidate):
        if output.get("status") == "TODO":
            return 2, "TODO: " + output["diagnostics"][0]["message"]
        if code or output.get("status") != "OK":
            return 1, "ERROR: runner did not complete successfully"
        if not output.get("snapshot_complete"):
            return 2, "TODO: snapshot cannot establish full parity (anonymous or unreflected state)"
    expected = json.dumps(comparable(oracle[1]), indent=2, sort_keys=True).splitlines()
    actual = json.dumps(comparable(candidate[1]), indent=2, sort_keys=True).splitlines()
    if expected != actual:
        return 1, "\n".join(difflib.unified_diff(expected, actual, fromfile="flecs", tofile="siecs"))
    return 0, "PASS"


def main():
    directory = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("request", nargs="?", type=Path, help="JSON input file; defaults to stdin")
    parser.add_argument("--oracle", type=Path, default=directory / "flecs_script_oracle")
    parser.add_argument("--candidate", type=Path, default=directory / "siecs_script_candidate")
    args = parser.parse_args()
    try:
        request = json.loads(args.request.read_text()) if args.request else json.load(sys.stdin)
        code, message = compare(run(args.oracle, request), run(args.candidate, request))
        print(message)
        return code
    except (OSError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
