#!/usr/bin/env python3
"""Build the frozen external Flecs oracle without touching Bake's install paths."""

import argparse
import hashlib
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from sync_flecs_script_tests import FLECS_COMMIT, ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkout", type=Path)
    parser.add_argument("--build-dir", type=Path, default=ROOT / ".cache/flecs-script-oracle")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    checkout = args.checkout.resolve()
    revision = subprocess.check_output(["git", "-C", str(checkout), "rev-parse", "HEAD"], text=True).strip()
    if revision != FLECS_COMMIT:
        parser.error(f"checkout must be detached at {FLECS_COMMIT}, got {revision}")
    dirty = subprocess.check_output(["git", "-C", str(checkout), "status", "--porcelain",
                                     "--untracked-files=all"], text=True)
    if dirty:
        parser.error("oracle requires an unmodified upstream checkout")
    build = args.build_dir.resolve()
    build.mkdir(parents=True, exist_ok=True)
    compiler = shlex.split(os.environ.get("CC", "cc"))
    subprocess.run(["cmake", "-S", str(checkout), "-B", str(build / "flecs"),
                    "-DCMAKE_BUILD_TYPE=Release", "-DFLECS_SHARED=OFF", "-DFLECS_STATIC=ON",
                    "-DFLECS_TESTS=OFF"], check=True)
    subprocess.run(["cmake", "--build", str(build / "flecs"), "--parallel", str(args.jobs)], check=True)
    subprocess.run([*compiler, "-std=c99", "-Dflecs_STATIC", "-D_POSIX_C_SOURCE=200809L",
                    "-Wall", "-Wextra", "-Werror", "-I" + str(checkout / "include"),
                    str(ROOT / "tools/compat/oracle.c"), str(build / "flecs/libflecs_static.a"),
                    "-lpthread", "-lm", "-ldl", "-o", str(build / "oracle")], check=True)
    (build / "provenance.json").write_text(json.dumps({
        "commit": revision, "checkout": str(checkout), "dirty": False,
        "compiler": compiler, "source": "tools/compat/oracle.c",
        "source_sha256": hashlib.sha256((ROOT / "tools/compat/oracle.c").read_bytes()).hexdigest(),
        "binary_sha256": hashlib.sha256((build / "oracle").read_bytes()).hexdigest(),
    }, indent=2) + "\n")
    print(build / "oracle")


if __name__ == "__main__":
    main()
