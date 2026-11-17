"""Regression checks for coverage scope, missing files and the strict threshold."""
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("shop_coverage", Path(__file__).with_name("coverage.py"))
COVERAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COVERAGE)


class CoverageTests(unittest.TestCase):
    def test_persistence_scope_is_independent_of_core_coverage(self):
        data=self.report(10)
        data["data"][0]["files"].append({"filename":str(COVERAGE.LAB / "persistence/SnapshotStore.cpp"),
            "summary":{"lines":{"count":10,"covered":9}}})
        with patch("builtins.print"):
            self.assertTrue(COVERAGE.summarize(data,[],scope="core"))
            self.assertFalse(COVERAGE.summarize(data,[],scope="persistence"))
    def report(self, covered, count=10):
        return {"data": [{"files": [
            {"filename": str(COVERAGE.LAB / "src/common/Money.cpp"),
             "summary": {"lines": {"count": count, "covered": covered}}},
            {"filename": str(COVERAGE.LAB.parent / "lab2/src/core/Money.cpp"),
             "summary": {"lines": {"count": 1000, "covered": 1000}}}]}]}

    def test_threshold_is_strict_and_other_laboratories_are_excluded(self):
        with patch("builtins.print"):
            self.assertFalse(COVERAGE.summarize(self.report(9), []))
            self.assertTrue(COVERAGE.summarize(self.report(10), []))
            with self.assertRaises(ValueError):
                COVERAGE.summarize(self.report(0, 0), [])

    def test_missing_and_duplicate_mappings_fail(self):
        with self.assertRaises(ValueError):
            COVERAGE.summarize(self.report(10), [COVERAGE.LAB / "src/missing.cpp"])
        data = self.report(10)
        data["data"][0]["files"].append(data["data"][0]["files"][0])
        with self.assertRaises(ValueError):
            COVERAGE.summarize(data, [])


if __name__ == "__main__":
    unittest.main()
