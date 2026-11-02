#!/usr/bin/env python3
"""Run unchanged upstream script sources through both isolated runtimes."""
import json
import unittest
from pathlib import Path
from compare import compare
from test_phase0 import invoke

ROOT = Path(__file__).resolve().parents[2]


class CandidateTests(unittest.TestCase):
    def test_upstream_entity_sources(self):
        fixtures = json.loads((ROOT / "addons/script/test/compat/eval_entities.json").read_text())
        for fixture in fixtures["cases"]:
            for mode in ("parse", "run"):
                with self.subTest(case=fixture["id"], mode=mode):
                    request = {"source": fixture["source"], "mode": mode}
                    oracle = invoke("flecs_script_oracle", request)
                    candidate = invoke("siecs_script_candidate", request)
                    self.assertTrue(oracle[1]["success"], oracle[1])
                    code, difference = compare(oracle, candidate)
                    self.assertEqual(code, 0, difference)

    def test_interpolated_loop_names_and_variables(self):
        sources = [
            'for i in 0..4 {\n "Entity_{$i * 2}" {}\n}\n',
            'const threshold = 3\nif $threshold > 2 { Present {} } else { Absent {} }',
            'Active {}\nShip { Active }',
            'Root { Child {} }\nRoot { Another {} }',
            'for i in 0..5 { if $i == 2 { continue }\n "Entity_$i" {} }',
            'if (5 & 3) == 1 { BitAnd {} }\nif (1 << 4) == 16 { Shift {} }',
            'if "same" == "same" { Equal {} }\nif "a" != "b" { Unequal {} }',
        ]
        for source in sources:
            with self.subTest(source=source):
                request = {"source": source, "mode": "run"}
                oracle = invoke("flecs_script_oracle", request)
                candidate = invoke("siecs_script_candidate", request)
                self.assertTrue(oracle[1]["success"], oracle[1])
                code, difference = compare(oracle, candidate)
                self.assertEqual(code, 0, difference)

    def test_unsupported_paths_remain_visible(self):
        for request in ({"source": "", "mode": "managed"}, {"source": "", "ir": True}):
            code, output = invoke("siecs_script_candidate", request)
            self.assertEqual(code, 2)
            self.assertEqual(output["status"], "TODO")

    def test_host_lifecycle_entities_are_not_hidden(self):
        code, output = invoke("siecs_script_candidate", {"source": "prefab Base {}"})
        self.assertEqual(code, 0)
        self.assertTrue(output["success"])
        self.assertFalse(output["snapshot_complete"])
        self.assertIn("Base", [entity["path"] for entity in output["entities"]])


if __name__ == "__main__":
    unittest.main(verbosity=2)
