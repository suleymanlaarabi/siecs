"""Process-level test protocol; this module never evaluates the script language."""

import json
import math
import re
import sys

from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from sync_flecs_script_tests import FLECS_COMMIT


def read_request():
    request = json.load(sys.stdin)
    if not isinstance(request, dict):
        raise ValueError("request must be a JSON object")
    if not isinstance(request.get("source"), str):
        raise ValueError("source must be a string")
    if "\0" in request["source"]:
        raise ValueError("embedded NUL is not supported by the C source API")
    request.setdefault("mode", "run")
    request.setdefault("events", [])
    if request["mode"] not in {"run", "parse", "managed", "reactive", "reload"}:
        raise ValueError("unknown execution mode")
    if not isinstance(request["events"], list):
        raise ValueError("events must be an array")
    for option in ("ir", "skip_unknown"):
        if option in request and not isinstance(request[option], bool):
            raise ValueError(f"{option} must be boolean")
    unknown = request.keys() - {"source", "mode", "events", "ir", "skip_unknown"}
    if unknown:
        raise ValueError("unsupported request fields: " + ", ".join(sorted(unknown)))
    return request


def gap(runner, message):
    return {
        "runner": runner, "upstream_commit": FLECS_COMMIT, "status": "TODO",
        "success": False, "snapshot_complete": False, "entities": [], "events": [],
        "diagnostics": [{"category": "compatibility_gap", "code": "TODO",
                         "message": message, "line": 0, "column": 0, "symbol": None}],
    }


def emit(result):
    print(json.dumps(result, sort_keys=True, ensure_ascii=False, allow_nan=False))


def fail(message):
    result = gap("protocol", message)
    result["status"] = "ERROR"
    result["diagnostics"][0].update(category="runner_error", code="INVALID_REQUEST")
    emit(result)
    return 2


def canonical_value(value):
    if isinstance(value, float) and not math.isfinite(value):
        raise ValueError("non-finite numbers require typed canonicalization")
    if isinstance(value, dict):
        return {key: canonical_value(item) for key, item in sorted(value.items())}
    if isinstance(value, list):
        return [canonical_value(item) for item in value]
    return value


def normalize(raw):
    """Normalize named-entity snapshots without dropping unrecognized host state."""
    entities = []
    complete = raw["snapshot_complete"]
    for record in raw["raw_entities"]:
        data = record["data"]
        unknown = data.keys() - {"name", "parent", "tags", "pairs", "components"}
        if unknown:
            raise ValueError("unhandled oracle entity fields: " + ", ".join(sorted(unknown)))
        path = record["path"]
        if re.search(r"(?:^|\.)#\d+(?:\.|$)", path):
            # Numeric IDs must never be compared across ECS implementations.
            complete = False
        components = {tag: None for tag in data.get("tags", [])}
        relations = []
        if "parent" in data:
            relations.append({"relation": "flecs.core.ChildOf", "target": data["parent"]})
        for relation, targets in data.get("pairs", {}).items():
            if not isinstance(targets, list):
                targets = [targets]
            for target in targets:
                relations.append({"relation": relation, "target": target})
        for name, value in data.get("components", {}).items():
            if name.startswith("(") and name.endswith(")"):
                pair = name[1:-1].split(",")
                if len(pair) != 2:
                    raise ValueError(f"ambiguous serialized pair: {name}")
                relations.append({"relation": pair[0], "target": pair[1], "value": value})
            else:
                components[name] = value
                if value is None:
                    # The upstream JSON serializer emits null for unreflected
                    # payloads. That is insufficient evidence for value parity.
                    complete = False
        entities.append({"path": path, "components": canonical_value(components),
                         "relations": sorted((canonical_value(r) for r in relations),
                                             key=lambda r: json.dumps(r, sort_keys=True))})
    diagnostics = [{**item, "symbol": None} for item in raw["diagnostics"]]
    return {"runner": "flecs", "upstream_commit": FLECS_COMMIT, "status": "OK",
            "success": raw["success"], "snapshot_complete": complete,
            "entities": sorted(entities, key=lambda e: e["path"]),
            "diagnostics": diagnostics, "events": []}
