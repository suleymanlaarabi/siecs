#!/usr/bin/env python3
"""Infrastructure and oracle checks; these do not pass SiECS compatibility cases."""

import copy
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from compare import compare
from protocol import normalize

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from sync_flecs_script_tests import FLECS_COMMIT, ROOT, preserve_progress

DIRECTORY = Path(__file__).resolve().parent


def invoke(name, request):
    result = subprocess.run([str(DIRECTORY / name)], input=json.dumps(request),
                            text=True, capture_output=True, timeout=35)
    return result.returncode, json.loads(result.stdout)


class ManifestTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads((ROOT / "test/compat/flecs_manifest.json").read_text())

    def test_all_registrations_and_parameter_variants_retained(self):
        self.assertEqual(self.data["upstream"]["commit"], FLECS_COMMIT)
        self.assertEqual(len(self.data["suites"]), 34)
        self.assertEqual(sum(len(s["cases"]) for s in self.data["suites"]), 4130)
        ids = set()
        for suite in self.data["suites"]:
            self.assertEqual([c["name"] for c in suite["cases"]], suite["definition"]["testcases"])
            expected = 1
            for values in suite["definition"].get("params", {}).values():
                expected *= len(values)
            self.assertEqual(len(suite["variants"]), expected)
            for case in suite["cases"]:
                self.assertNotIn(case["id"], ids)
                ids.add(case["id"])
                self.assertGreater(case["upstream_line"], 0)
        self.assertIn("Error.component_in_with_scope#2", ids)

    def test_every_script_blob_is_audited(self):
        expected = {p for p in self.data["inventory"] if p.startswith("src/addons/script/")}
        self.assertEqual({f["upstream_path"] for f in self.data["script_files"]}, expected)
        for file in self.data["script_files"]:
            self.assertEqual(file["upstream_blob"], self.data["inventory"][file["upstream_path"]])
        self.assertIn("EcsAstTry", self.data["ast_nodes"])
        self.assertIn("EcsAstWhile", self.data["ast_nodes"])
        self.assertIn("ecs_script_task_new", {a["name"] for a in self.data["public_api"]})

    def test_progress_preserved_and_unsupported_claim_rejected(self):
        old = copy.deepcopy(self.data)
        case = old["suites"][0]["cases"][0]
        case["status"]["parser"] = "BLOCKED_CORE"
        case["evidence"]["parser"] = "Independent backend case and blocker description"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "manifest.json"
            path.write_text(json.dumps(old))
            target = copy.deepcopy(self.data)
            preserve_progress(target, path)
            self.assertEqual(target["suites"][0]["cases"][0], case)
            case["evidence"] = {}
            path.write_text(json.dumps(old))
            with self.assertRaisesRegex(ValueError, "requires evidence"):
                preserve_progress(copy.deepcopy(self.data), path)


class ComparatorTests(unittest.TestCase):
    def test_unimplemented_managed_candidate_cannot_pass_even_empty_source(self):
        code, candidate = invoke("siecs_script_candidate", {"source": "", "mode": "managed"})
        self.assertEqual(code, 2)
        self.assertEqual(candidate["status"], "TODO")
        self.assertFalse(candidate["success"])
        self.assertEqual(compare((0, candidate), (code, candidate))[0], 2)

    def test_malformed_requests_fail(self):
        for request in ({}, {"source": 42}, {"source": "e {}", "ir": "yes"},
                        {"source": "e {}", "unknown": True}, {"source": "\0"}):
            with self.subTest(request=request):
                code, result = invoke("siecs_script_candidate", request)
                self.assertEqual(code, 2)
                self.assertEqual(result["status"], "ERROR")

    def test_changed_values_and_missing_state_fail_comparison(self):
        result = {"status": "OK", "snapshot_complete": True, "success": True,
                  "entities": [{"path": "e", "components": {"Mass": 10}, "relations": []}],
                  "events": [], "diagnostics": []}
        other = copy.deepcopy(result)
        other["entities"][0]["components"]["Mass"] = 20
        self.assertEqual(compare((0, result), (0, other))[0], 1)
        other = copy.deepcopy(result)
        other["snapshot_complete"] = False
        self.assertEqual(compare((0, result), (0, other))[0], 2)
        self.assertEqual(compare((0, result), (0, result)), (0, "PASS"))

    def test_unknown_snapshot_fields_are_never_discarded(self):
        with self.assertRaisesRegex(ValueError, "unhandled oracle"):
            normalize({"success": True, "snapshot_complete": True, "diagnostics": [],
                       "raw_entities": [{"path": "e", "data": {"unknown_state": 42}}]})


class OracleTests(unittest.TestCase):
    def execute(self, source, **options):
        code, result = invoke("flecs_script_oracle", {"source": source, **options})
        self.assertEqual(code, 0, result)
        self.assertEqual(result["status"], "OK", result)
        return result

    def test_named_hierarchy_and_tags_match_ast_ir(self):
        source = "Tag {}\nRoot {\n Child { Tag }\n}\n"
        ast = self.execute(source)
        ir = self.execute(source, ir=True)
        self.assertEqual(ast, ir)
        self.assertTrue(ast["snapshot_complete"])
        by_path = {e["path"]: e for e in ast["entities"]}
        self.assertEqual(set(by_path), {"Tag", "Root", "Root.Child"})
        self.assertEqual(by_path["Root.Child"]["components"], {"Tag": None})
        self.assertEqual(by_path["Root.Child"]["relations"],
                         [{"relation": "flecs.core.ChildOf", "target": "Root"}])

    def test_typed_values_and_both_pair_type_positions(self):
        source = ("struct Position(x: f32, y: f32)\nTarget {}\n"
                  "e { Position: {1,2}; (Position, Target): {3,4}; (Target, Position): {5,6} }")
        for ir in (False, True):
            with self.subTest(ir=ir):
                result = self.execute(source, ir=ir)
                self.assertTrue(result["success"])
                entity = next(e for e in result["entities"] if e["path"] == "e")
                self.assertEqual(entity["components"]["Position"], {"x": 1, "y": 2})
                pairs = {(r["relation"], r["target"]): r["value"] for r in entity["relations"]}
                self.assertEqual(pairs, {("Position", "Target"): {"x": 3, "y": 4},
                                         ("Target", "Position"): {"x": 5, "y": 6}})
                # Unreflected upstream type-serializer state is visible, not
                # silently stripped to make host metadata parity appear green.
                self.assertFalse(result["snapshot_complete"])

    def test_diagnostics_have_source_location_and_do_not_corrupt_json(self):
        result = self.execute('"entity \\\"quote" {')
        self.assertFalse(result["success"])
        self.assertEqual(result["diagnostics"][0]["line"], 1)
        self.assertGreater(result["diagnostics"][0]["column"], 0)
        self.assertTrue(result["diagnostics"][0]["message"])

    def test_parse_does_not_create_script_entities(self):
        result = self.execute("Root { Child {} }", mode="parse")
        self.assertTrue(result["success"])
        self.assertEqual(result["entities"], [])

    def test_mutated_preexisting_entities_make_snapshot_incomplete(self):
        result = self.execute("$ { flecs.meta.i32: 10 }")
        self.assertTrue(result["success"])
        self.assertFalse(result["snapshot_complete"])

    def test_anonymous_snapshot_and_unsupported_modes_cannot_pass(self):
        result = self.execute("{ Child {} }")
        self.assertFalse(result["snapshot_complete"])
        for mode in ("managed", "reactive", "reload"):
            code, result = invoke("flecs_script_oracle", {"source": "", "mode": mode})
            self.assertEqual(code, 2)
            self.assertEqual(result["status"], "TODO")

    def test_empty_world_and_multiple_relation_targets(self):
        self.assertEqual(self.execute("")["entities"], [])
        result = self.execute("Likes {}\nPizza {}\nSalad {}\ne { (Likes, Pizza); (Likes, Salad) }")
        entity = next(e for e in result["entities"] if e["path"] == "e")
        self.assertEqual(entity["relations"], [{"relation": "Likes", "target": "Pizza"},
                                               {"relation": "Likes", "target": "Salad"}])


if __name__ == "__main__":
    unittest.main(verbosity=2)
