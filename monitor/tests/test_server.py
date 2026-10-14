"""Run from the project root: python -m unittest discover -s monitor/tests -v"""

import copy
import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("world_monitor_server", ROOT / "server.py")
server = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(server)


class SnapshotTests(unittest.TestCase):
    def setUp(self):
        self.data = server.decode((ROOT / "examples/village.json").read_bytes())
        self.now = 0
        self.store = server.Store(lambda: self.now)

    def test_examples(self):
        for path in (ROOT / "examples").glob("*.json"):
            server.decode(path.read_bytes())

    def test_counts(self):
        self.assertEqual(self.store.accept(self.data, 100), 202)
        world = self.store.state()["worlds"][0]
        self.assertEqual(world["summary"]["counts"]["npc"], {"alive": 1, "total": 1})
        self.assertEqual(world["settlements"]["town"]["counts"]["monster"]["total"], 0)

    def test_replace_and_sequence(self):
        self.store.accept(self.data, 100)
        self.assertEqual(self.store.accept(self.data, 100), 409)
        data = copy.deepcopy(self.data)
        data.update(sequence=2, entities=[])
        self.assertEqual(self.store.accept(data, 100), 202)
        self.assertEqual(self.store.state()["worlds"][0]["summary"]["counts"]["npc"]["total"], 0)

    def test_epoch_and_stream_identity(self):
        self.store.accept(self.data, 100)
        data = copy.deepcopy(self.data)
        data.update(sequence=2, epoch=0)
        self.assertEqual(self.store.accept(data, 100), 409)
        data.update(epoch=2)
        self.assertEqual(self.store.accept(data, 100), 202)
        data = copy.deepcopy(self.data)
        data["instance_id"] = "second-process"
        self.assertEqual(self.store.accept(data, 100), 202)
        self.assertEqual(len(self.store.state()["worlds"]), 2)

    def test_stale_preserves_counts_and_expires(self):
        self.store.accept(self.data, 100)
        self.now = 6
        world = self.store.state()["worlds"][0]
        self.assertTrue(world["stale"])
        self.assertEqual(world["summary"]["counts"]["npc"]["alive"], 1)
        self.now = server.RETENTION_SECONDS
        self.assertEqual(self.store.state()["worlds"], [])

    def test_invalid_entities(self):
        for change in ({"health": float("nan")}, {"health": True}, {"settlement_id": "unknown"},
                       {"kind": "dragon"}, {"alive": 1}, {"max_health": -1}):
            data = copy.deepcopy(self.data)
            data["entities"][0].update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                server.validate(data)

    def test_duplicate_and_nonfinite_json(self):
        self.data["entities"].append(copy.deepcopy(self.data["entities"][0]))
        with self.assertRaises(ValueError):
            server.validate(self.data)
        for body in (b'{"x":1,"x":2}', b'{"x":NaN}', b'{"x":Infinity}'):
            with self.subTest(body=body), self.assertRaises(ValueError):
                server.decode(body)

    def test_capacity(self):
        for i in range(server.MAX_WORLDS):
            data = copy.deepcopy(self.data)
            data["instance_id"] = str(i)
            self.assertEqual(self.store.accept(data, 100), 202)
        self.assertEqual(self.store.accept(self.data, 100), 429)

    def test_unsafe_extension_fields(self):
        original = (ROOT / "examples/village.json").read_bytes().rstrip()
        for extension in (b',"extra":1e999}', b',"extra":"\\ud800"}'):
            with self.subTest(extension=extension), self.assertRaises((ValueError, UnicodeError)):
                server.decode(original[:-1] + extension)

    def test_dead_and_hostile_npc_counts(self):
        entity = self.data["entities"][0]
        entity.update(hostile=True, health=20)
        summary = server.summarize([entity])
        self.assertEqual(summary["npc_critical"], 1)
        self.assertEqual(summary["npc_hostile"], 1)
        entity.update(alive=False, health=0)
        summary = server.summarize([entity])
        self.assertEqual(summary["counts"]["npc"], {"alive": 0, "total": 1})
        self.assertEqual(summary["npc_critical"], 0)


if __name__ == "__main__":
    unittest.main()
