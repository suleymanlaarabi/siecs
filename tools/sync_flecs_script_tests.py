#!/usr/bin/env python3
"""Freeze the Flecs Script backlog and port inventory from immutable git blobs."""

import argparse
import hashlib
import itertools
import json
import re
import subprocess
import sys
from pathlib import Path


FLECS_COMMIT = "fa788c60c87b4effc1124027db86ef0f30a778d4"
SIECS_COMMIT = "c41a4a220e2d6fabfa1c3cf7c0461647c5c765fd"
ROOT = Path(__file__).resolve().parents[1]
DIMENSIONS = (
    "parser", "AST", "eval AST", "eval IR", "managed", "reactive",
    "template", "async", "edit/source", "C API", "C++ API", "WASM",
)
STATUSES = {"PASS", "TODO", "BLOCKED_CORE", "BLOCKED_REFLECT", "HOST_DIFFERENCE"}
PUBLIC_HEADERS = (
    "include/flecs/addons/script.h",
    "include/flecs/addons/script_async.h",
)

# This is a file-by-file integration audit, not a list of language features.
# No entry means an upstream inventory change requires an explicit review.
PLATFORM_FILES = {
    "ast.c", "ast.h", "parser.c", "visit.h", "visit_free.c", "visit_to_str.c",
    "edit.c", "expr/ast.c", "expr/ast.h", "expr/expr.h", "expr/format.c",
    "expr/parser.c", "expr/stack.c", "expr/stack.h", "expr/util.c",
    "expr/visit.h", "expr/visit_eval.c", "expr/visit_fold.c",
    "expr/visit_free.c", "expr/visit_refs.c", "expr/visit_to_str.c",
    "ir/compile.c", "ir/ir.h", "ir/to_str.c", "reactivity/deps.c",
    "reactivity/deps.h", "modules/functions_math.c",
    "modules/functions_math_perlin.c", "modules/platform.c",
    "eval/visit_for.c",
}
BACKEND_FILES = {
    "script.c", "script.h", "vars.c", "function.c", "functions_builtin.c",
    "serialize.c", "serialize_type.c", "eval/async.c", "eval/async.h",
    "eval/eval.h", "eval/runtime.c", "eval/runtime.h", "eval/visit_collection.c",
    "eval/visit_enum.c", "eval/visit_eval.c", "eval/visit_eval.h",
    "eval/visit_include.c", "eval/visit_struct.c", "eval/visit_type.c",
    "eval/visit_type.h", "expr/visit_type.c", "ir/vm.c",
    "reactivity/refs.c", "reactivity/refs.h", "reactivity/state.c",
    "reactivity/state.h", "reactivity/template.c", "reactivity/template.h",
    "modules/command.c", "modules/command.h", "modules/command_core.c",
    "modules/command_diff.c", "modules/command_input.c",
    "modules/command_script.c", "modules/input.c", "modules/typecache.c",
}


def git(checkout, *args):
    return subprocess.check_output(["git", "-C", str(checkout), *args])


def blob(checkout, path):
    return git(checkout, "show", f"{FLECS_COMMIT}:{path}")


def tree(checkout, *paths):
    entries = {}
    for line in git(checkout, "ls-tree", "-r", FLECS_COMMIT, "--", *paths).decode().splitlines():
        meta, path = line.split("\t", 1)
        entries[path] = meta.split()[2]
    return entries


def strip_comments(source):
    # Keep literals intact, including URLs and strings containing comment markers.
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*'
    return re.sub(pattern, lambda m: " " if m[0].startswith(("/*", "//")) else m[0],
                  source, flags=re.S)


def public_api(source):
    result = []
    for declaration in re.findall(r"\bFLECS_API\s+([^;]+);", strip_comments(source), re.S):
        declaration = " ".join(declaration.split())
        names = re.findall(r"\b(ecs_[a-zA-Z0-9_]+)\s*\(", declaration)
        if names:
            result.append({"name": names[0], "declaration": declaration + ";"})
    return result


def variants(suite):
    params = suite.get("params", {})
    return [dict(zip(params, values)) for values in itertools.product(*params.values())]


def collect(checkout):
    revision = git(checkout, "rev-parse", FLECS_COMMIT + "^{commit}").decode().strip()
    if revision != FLECS_COMMIT:
        raise ValueError("unexpected Flecs revision")
    entries = tree(checkout, "src/addons/script", "src/addons/parser", *PUBLIC_HEADERS,
                   "include/flecs/addons/cpp/mixins/script", "test/script", "LICENSE",
                   "docs/FlecsScript.md")
    project_path = "test/script/project.json"
    project = json.loads(blob(checkout, project_path))
    suites = []
    occurrences = {}
    for suite in project["test"]["testsuites"]:
        suite_id = suite["id"]
        source_path = f"test/script/src/{suite_id}.c"
        if source_path not in entries:
            raise ValueError(f"missing upstream suite source: {source_path}")
        source = blob(checkout, source_path).decode()
        cases = []
        for index, case in enumerate(suite["testcases"]):
            test_name = f"{suite_id}.{case}"
            occurrence = occurrences.get(test_name, 0) + 1
            occurrences[test_name] = occurrence
            case_id = test_name if occurrence == 1 else f"{test_name}#{occurrence}"
            match = re.search(r"\bvoid\s+" + re.escape(f"{suite_id}_{case}") + r"\s*\(", source)
            if not match:
                raise ValueError(f"test definition not found: {case_id}")
            cases.append({
                "id": case_id, "name": case, "manifest_index": index,
                "occurrence": occurrence,
                "upstream_line": source.count("\n", 0, match.start()) + 1,
                "status": {dimension: "TODO" for dimension in DIMENSIONS},
                "evidence": {}, "notes": "",
            })
        suites.append({
            "id": suite_id, "upstream_path": source_path,
            "upstream_blob": entries[source_path],
            "definition": suite, "variants": variants(suite), "cases": cases,
        })
    script_files = []
    for path, sha in entries.items():
        if not path.startswith("src/addons/script/"):
            continue
        relative = path.removeprefix("src/addons/script/")
        if relative in PLATFORM_FILES:
            classification = "PORT_WITH_PLATFORM_ADAPTER"
            notes = ("Retain upstream algorithms; adapt containers/allocator/parser utilities. "
                     "Route any ECS/meta calls through the typed SiECS backend.")
        elif relative in BACKEND_FILES:
            classification = "BACKEND_REWRITE"
            notes = ("Retain language/control/dependency algorithms; replace world, ID, meta, "
                     "observer or module integration with SiECS/sireflect. "
                     "This classification does not authorize a frontend rewrite.")
        else:
            raise ValueError(f"unreviewed upstream file: {path}")
        source = blob(checkout, path).decode()
        uncommented = strip_comments(source)
        script_files.append({
            "upstream_path": path, "upstream_blob": sha,
            "local_path": "addons/script/src/" + relative,
            "classification": classification,
            "includes": re.findall(r'^\s*#\s*include\s*[<"]([^>"\n]+)', source, re.M),
            "referenced_identifiers": sorted(set(re.findall(
                r"\b(?:ecs|flecs)_[A-Za-z0-9_]+\b", uncommented))),
            "status": "TODO", "notes": notes,
        })
    apis = []
    for header in PUBLIC_HEADERS:
        for api in public_api(blob(checkout, header).decode()):
            apis.append({**api, "upstream_path": header, "status": "TODO",
                         "evidence": "", "host_difference": ""})
    nodes = re.search(r"typedef enum ecs_script_node_kind_t\s*\{(.*?)\}",
                      blob(checkout, "src/addons/script/ast.h").decode(), re.S)[1]
    return {
        "schema": 1,
        "upstream": {"repository": "https://github.com/SanderMertens/flecs",
                     "commit": FLECS_COMMIT, "manifest_path": project_path,
                     "manifest_blob": entries[project_path], "siecs_baseline": SIECS_COMMIT,
                     "license_sha256": hashlib.sha256(blob(checkout, "LICENSE")).hexdigest()},
        "dimensions": DIMENSIONS,
        "inventory": entries,
        "script_files": script_files,
        "ast_nodes": re.findall(r"\bEcsAst\w+", nodes),
        "public_api": apis,
        "suites": suites,
    }


def preserve_progress(data, path):
    if not path.exists():
        return
    previous = json.loads(path.read_text())
    if previous["upstream"]["commit"] != FLECS_COMMIT:
        raise ValueError("baseline changed; review and sync explicitly")
    old_cases = {case["id"]: case for suite in previous["suites"] for case in suite["cases"]}
    new_ids = {case["id"] for suite in data["suites"] for case in suite["cases"]}
    if old_cases.keys() - new_ids:
        raise ValueError("testcase removal requires an explicit baseline review")
    for suite in data["suites"]:
        for case in suite["cases"]:
            old = old_cases.get(case["id"])
            if not old:
                continue
            for dimension, status in old["status"].items():
                if dimension not in DIMENSIONS or status not in STATUSES:
                    raise ValueError(f"invalid status: {case['id']}/{dimension}/{status}")
                evidence = old.get("evidence", {}).get(dimension)
                if status != "TODO" and not evidence:
                    raise ValueError(f"status requires evidence: {case['id']}/{dimension}")
                case["status"][dimension] = status
            case["evidence"] = old.get("evidence", {})
            case["notes"] = old.get("notes", "")
    old_apis = {api["name"]: api for api in previous["public_api"]}
    for api in data["public_api"]:
        old = old_apis.get(api["name"])
        if old:
            api.update({key: old[key] for key in ("status", "evidence", "host_difference")})
            if api["status"] not in STATUSES:
                raise ValueError(f"invalid API status: {api['name']}")
            if api["status"] != "TODO" and not api["evidence"]:
                raise ValueError(f"API status requires evidence: {api['name']}")
            if api["status"] == "HOST_DIFFERENCE" and not api["host_difference"]:
                raise ValueError(f"host difference requires justification: {api['name']}")


def compat_markdown(data):
    suites = data["suites"]
    case_count = sum(len(s["cases"]) for s in suites)
    variant_count = sum(len(s["cases"]) * len(s["variants"]) for s in suites)
    lines = [
        "# Flecs Script compatibility", "",
        "Generated by `tools/sync_flecs_script_tests.py`; edit statuses/evidence in "
        "`test/compat/flecs_manifest.json` and regenerate. No passing status is inferred "
        "from a build, an oracle result, or another dimension.", "",
        f"- Flecs baseline: `{FLECS_COMMIT}`.",
        f"- SiECS baseline: `{SIECS_COMMIT}`.",
        f"- Coverage inventory: **{len(suites)} suites, {case_count} testcases, "
        f"{variant_count} parameterized executions**.",
        "- Statuses: `PASS`, `TODO`, `BLOCKED_CORE`, `BLOCKED_REFLECT`, `HOST_DIFFERENCE`.",
        "- Phase 0 only: the SiECS Script runtime is not implemented. All language/API "
        "entries remain TODO. The candidate runner reports a gap and cannot pass parity.",
        "- TODO in a dimension also covers applicability not yet audited; it never means PASS.",
        "- Every non-TODO status requires evidence; HOST_DIFFERENCE additionally requires "
        "a precise justification. Test parameters are retained verbatim in the manifest.", "",
        "Duplicate testcase registrations are retained with occurrence IDs (`#2`, etc.); "
        "counts reflect the upstream manifest, not deduplicated function names.", "",
        "## Upstream suites", "",
        "| Suite | Cases | Executions | Parameters |", "| --- | ---: | ---: | --- |",
    ]
    for suite in suites:
        count = len(suite["cases"])
        params = json.dumps(suite["definition"].get("params", {}))
        lines.append(f"| {suite['id']} | {count} | {count * len(suite['variants'])} | `{params}` |")
    lines += ["", "## AST feature inventory", "",
              "Taken from `src/addons/script/ast.h`, including features absent from illustrative lists.", "",
              ", ".join(f"`{node}`" for node in data["ast_nodes"]), "",
              "## Public C API", "",
              "Declarations from both pinned public headers. Signature adaptations are recorded "
              "in each manifest entry's `host_difference` field.", "",
              "| Function | Status | Host difference |", "| --- | --- | --- |"]
    for api in data["public_api"]:
        lines.append(f"| `{api['name']}` | {api['status']} | {api['host_difference']} |")
    for suite in suites:
        path = suite["upstream_path"]
        lines += ["", f"## {suite['id']}", "",
                  f"Upstream: `{path}` (blob `{suite['upstream_blob']}`).", "",
                  "| Test | " + " | ".join(DIMENSIONS) + " |",
                  "| --- | " + " | ".join("---" for _ in DIMENSIONS) + " |"]
        for case in suite["cases"]:
            statuses = " | ".join(case["status"][d] for d in DIMENSIONS)
            name = case['name'] + (f"#{case['occurrence']}" if case['occurrence'] > 1 else "")
            lines.append(f"| `{name}` | {statuses} |")
    return "\n".join(lines) + "\n"


def port_markdown(data):
    lines = [
        "# Flecs Script port map", "",
        f"Pinned source: `{FLECS_COMMIT}`. SiECS checkout: `{SIECS_COMMIT}`.", "",
        "Generated before runtime implementation. Classifications identify the dominant "
        "integration boundary, not permission to replace the upstream frontend algorithms. "
        "No file is NOT_NEEDED and no source file has been ported yet.", "",
        "The manifest records every referenced `ecs_*`/`flecs_*` identifier per file "
        "(lexical inventory, including local declarations; not a linker-derived external "
        "symbol list). Includes below identify direct dependencies. World/meta calls in "
        "platform-adapted files still require backend routing. Headers containing entity "
        "IDs require typed native symbol adaptation, never a hidden Flecs world.", "",
        "Shared parser prerequisites `src/addons/parser/{grammar.h,parser.h,strutils.c,"
        "tokenizer.c,tokenizer.h}`, public headers, C++ mixins, documentation and test "
        "source blobs are separately frozen in the manifest inventory.", "",
        "| Upstream path | Upstream blob | Local path | Classification | Direct dependencies | Status | Notes |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]
    for entry in data["script_files"]:
        includes = ", ".join(f"`{inc}`" for inc in entry["includes"]) or "AST/backend types from includer"
        lines.append("| " + " | ".join((f"`{entry['upstream_path']}`",
                     f"`{entry['upstream_blob']}`", f"`{entry['local_path']}`",
                     entry["classification"], includes, entry["status"], entry["notes"])) + " |")
    return "\n".join(lines) + "\n"


def outputs(data, checkout):
    return {
        "test/compat/flecs_manifest.json": json.dumps(data, indent=2, ensure_ascii=False) + "\n",
        "docs/FLECS_SCRIPT_COMPAT.md": compat_markdown(data),
        "docs/FLECS_SCRIPT_PORT_MAP.md": port_markdown(data),
        "addons/script/LICENSE.flecs": blob(checkout, "LICENSE").decode(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkout", type=Path, help="Flecs git repository containing the pinned commit")
    parser.add_argument("--root", type=Path, default=ROOT, help="output repository (also useful for tests)")
    parser.add_argument("--check", action="store_true", help="check generated files without writing")
    args = parser.parse_args()
    try:
        data = collect(args.checkout)
        preserve_progress(data, args.root / "test/compat/flecs_manifest.json")
        stale = []
        for relative, content in outputs(data, args.checkout).items():
            destination = args.root / relative
            if args.check:
                if not destination.exists() or destination.read_bytes() != content.encode():
                    stale.append(relative)
            else:
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(content.encode())
        if stale:
            print("stale generated files: " + ", ".join(stale), file=sys.stderr)
            return 1
        print(f"{'checked' if args.check else 'generated'} {len(data['suites'])} suites, "
              f"{sum(len(s['cases']) for s in data['suites'])} tests, "
              f"{len(data['script_files'])} script files, {len(data['public_api'])} C declarations")
        return 0
    except (ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
