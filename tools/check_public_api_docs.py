#!/usr/bin/env python3
"""Check that exported C and typed C++ declarations have real documentation.

The C++ half uses clang-doc's AST, so overloads, templates and methods are
checked individually rather than by a hand-maintained name list. The C half
also checks function-like macros, which are not represented by clang-doc.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import tempfile
import yaml

ROOT = Path(__file__).resolve().parents[1]

def clang_doc_command():
    configured = os.environ.get("CLANG_DOC")
    if configured:
        return configured

    direct = shutil.which("clang-doc")
    if direct:
        return direct

    versioned = []
    for directory in os.environ.get("PATH", "").split(os.pathsep):
        for candidate in Path(directory or ".").glob("clang-doc-*"):
            match = re.fullmatch(r"clang-doc-(\d+(?:\.\d+)*)", candidate.name)
            if candidate.is_file() and os.access(candidate, os.X_OK) and match:
                version = tuple(int(part) for part in match.group(1).split("."))
                versioned.append((version, candidate.name))
    for _, name in sorted(set(versioned), reverse=True):
        found = shutil.which(name)
        if found:
            return found

    raise FileNotFoundError(
        "clang-doc not found; install clang-tools (or set CLANG_DOC to its path)"
    )

def run_clang_doc(inputs, cxx):
    with tempfile.TemporaryDirectory(prefix="siecs-api-docs-") as out:
        cmd = [clang_doc_command(), *map(str, inputs), "--format=yaml", "--output", out,
               "--public", "--doxygen", "--extra-arg=-Iinclude",
               "--extra-arg=-DSIECS_CUSTOM_BUILD", "--extra-arg=-DSICORE_VEC=0"]
        if cxx:
            cmd += ["--extra-arg=-x", "--extra-arg=c++", "--extra-arg=-std=c++23"]
        else:
            cmd += ["--extra-arg=-x", "--extra-arg=c"]
        result = subprocess.run(cmd, cwd=ROOT, text=True, capture_output=True)
        if result.returncode:
            raise RuntimeError(result.stderr)
        for path in Path(out).rglob("*.yaml"):
            try:
                yield yaml.safe_load(path.read_text())
            except yaml.YAMLError:
                continue

def documented(info):
    description = info.get("Description")
    return bool(description and description.get("ParagraphComments"))

def cpp_missing():
    missing = []
    for info in run_clang_doc(sorted((ROOT / "include/siecs/cpp").glob("*.hpp")), True):
        location = info.get("Location", {})
        filename = location.get("Filename", "")
        if "/include/siecs/cpp/" not in filename:
            continue
        namespace = info.get("Namespace", [])
        if "detail" in namespace:
            continue
        if info.get("InfoType") == "record" and info.get("Path") == "ecs":
            if not documented(info):
                missing.append((filename, location.get("LineNumber"), info.get("Name")))
            for method in info.get("PublicMethods", []):
                if not documented(method):
                    missing.append((filename, method.get("Location", {}).get("LineNumber"),
                                    f"{info.get('Name')}::{method.get('Name')}"))
        if info.get("InfoType") == "namespace" and info.get("Name") == "ecs":
            for function in info.get("Functions", []):
                if "detail" not in function.get("Namespace", []) and not documented(function):
                    missing.append((filename, function.get("Location", {}).get("LineNumber"),
                                    function.get("Name")))
    return missing

def c_missing():
    text = (ROOT / "include/siecs.h").read_text()
    lines = text.splitlines()
    missing = []
    def has_adjacent_comment(index):
        j = index - 1
        while j >= 0 and (not lines[j].strip() or lines[j].lstrip().startswith(
            ("#if", "#else", "#elif", "#endif")
        )):
            j -= 1
        if j < 0:
            return False
        if lines[j].lstrip().startswith("//"):
            return True
        if "*/" not in lines[j]:
            return False
        while j >= 0:
            if "/*" in lines[j]:
                return True
            j -= 1
        return False
    for i, line in enumerate(lines):
        if line.lstrip().startswith(("#", "/*", "*", "//")):
            continue
        continuation = bool(re.match(r"\s*ecs_[A-Za-z0-9_]+\s*\(", line))
        declaration = bool(re.search(r"\b(?:SIECS_API\s+)?[\w\s\*]+\becs_[A-Za-z0-9_]+\s*\(", line))
        if line.lstrip().startswith(("return", "if", "for", "while")) or not (declaration or continuation):
            continue
        if continuation and i:
            previous = lines[i - 1].strip()
            if not previous.startswith("SIECS_API") and not previous.endswith(("*", "const")):
                continue
        macro = any(lines[j].lstrip().startswith("#define") and lines[j].rstrip().endswith("\\")
                    for j in range(max(0, i - 12), i))
        comment_index = i - 1 if continuation else i
        if not macro and not has_adjacent_comment(comment_index):
            missing.append(("include/siecs.h", i + 1, line.strip()))
    for i, line in enumerate(lines):
        match = re.match(r"\s*#define\s+(ecs_[A-Za-z0-9_]+|ECS_[A-Za-z0-9_]+|On[A-Za-z0-9_]+|EcsOn[A-Za-z0-9_]+)\b", line)
        if match and not has_adjacent_comment(i):
            missing.append(("include/siecs.h", i + 1, match.group(1)))
    for i, line in enumerate(lines):
        if not line.lstrip().startswith("typedef "):
            continue
        macro = any(lines[j].lstrip().startswith("#define") and lines[j].rstrip().endswith("\\")
                    for j in range(max(0, i - 12), i))
        if not macro and not has_adjacent_comment(i):
            missing.append(("include/siecs.h", i + 1, "typedef"))
    return missing

def main():
    try:
        missing = c_missing() + cpp_missing()
    except (OSError, RuntimeError) as error:
        print(f"public API documentation check could not run: {error}", file=sys.stderr)
        return 2
    if missing:
        print("Undocumented public declarations:", file=sys.stderr)
        for filename, line, name in missing:
            print(f"- {filename}:{line}: {name}", file=sys.stderr)
        return 1
    print("Public API documentation check passed (AST-checked C++ and C declarations).")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
