"""Tests for coverage scope/thresholds; these do not replace C++ domain tests."""
import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("lab2_coverage", Path(__file__).with_name("coverage.py"))
COVERAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COVERAGE)


class CoverageTests(unittest.TestCase):
    def result(self, covered, count, filename=None, with_ui=False):
        data = {"data": [{"files": [{"filename": str(filename or COVERAGE.LAB_ROOT / "src/core/Money.cpp"),
                "summary": {"lines": {"covered": covered, "count": count}}}]}]}
        with contextlib.redirect_stdout(io.StringIO()):
            return COVERAGE.report_coverage(data, with_ui)

    def test_strict_boundary_and_zero_lines(self):
        self.assertFalse(self.result(90, 100))
        self.assertFalse(self.result(899, 1000))
        self.assertTrue(self.result(91, 100))
        with self.assertRaises(ValueError):
            self.result(0, 0)

    def test_production_scope_in_both_modes(self):
        root = COVERAGE.LAB_ROOT
        for with_ui in (False, True):
            for path in ("src/core/Money.cpp", "include/post/core/Money.hpp"):
                self.assertTrue(COVERAGE.production_file(root / path, with_ui))
            for path in ("tests/cli/ApplicationTests.cpp", "tools/cli_tests.py", "src-other/a.cpp",
                         "../lab1/src/a.cpp", "../lab2-other/src/a.cpp", "app-other/a.cpp"):
                self.assertFalse(COVERAGE.production_file(root / path, with_ui))
        self.assertFalse(COVERAGE.production_file(root / "app/main.cpp"))
        self.assertTrue(COVERAGE.production_file(root / "app/main.cpp", True))
        with self.assertRaises(ValueError):
            self.result(100, 100, root / "tests/ignored.cpp", True)

    def test_missing_profiles_are_not_coverage_success(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):
                COVERAGE.merge_profiles(Path(directory), "must-not-run")

    def test_labels_never_include_external_cli_runs(self):
        with patch.object(COVERAGE.subprocess, "run") as run:
            COVERAGE.run_tests(Path("build"), Path("profiles"))
            self.assertIn("^lab2$", run.call_args.args[0])
            COVERAGE.run_tests(Path("build"), Path("profiles"), True)
            self.assertIn("^lab2($|-ui$)", run.call_args.args[0])
            self.assertNotIn("lab2-cli", " ".join(run.call_args.args[0]))

    def test_ui_export_includes_entry_point_object_even_without_main_execution(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ("post_core_tests", "post_ui_tests", "post_cli"):
                (root / name).touch()
            with patch.object(COVERAGE.subprocess, "run") as run:
                run.return_value.stdout = '{"data": []}'
                COVERAGE.export_coverage(root / "post_core_tests", root / "unit.profdata", "llvm-cov", True)
                args = run.call_args.args[0]
                self.assertIn(f"-object={root / 'post_ui_tests'}", args)
                self.assertIn(f"-object={root / 'post_cli'}", args)
                self.assertIn(str(COVERAGE.LAB_ROOT / "app/main.cpp"), args)
                self.assertFalse(any("tests/" in arg for arg in args))


if __name__ == "__main__":
    unittest.main()
