"""Keep lab1 profile collection separate from lab2 and tooling tests."""
import importlib.util
from pathlib import Path
import re
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location("lab1_coverage", Path(__file__).with_name("coverage.py"))
COVERAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COVERAGE)


class CoverageScopeTests(unittest.TestCase):
    def test_unit_only_runs_only_lab1_cpp_tests(self):
        with patch.object(COVERAGE.subprocess, "run") as run:
            COVERAGE.run_tests(Path("build"), Path("profiles"), unit_only=True)
            arguments, options = run.call_args
            command = arguments[0]
            pattern = command[command.index("-L") + 1]
            self.assertIsNotNone(re.fullmatch(pattern, "lab1-unit"))
            for label in ("lab1-cli", "lab1-tools", "lab2", "lab2-ui", "lab2-cli"):
                self.assertIsNone(re.fullmatch(pattern, label))
            self.assertIn("--no-tests=error", command)
            self.assertEqual(options["env"]["LLVM_PROFILE_FILE"], str(Path("profiles/%p-%m.profraw")))
            self.assertTrue(options["check"])

    def test_full_scope_adds_lab1_cli_but_not_other_labs(self):
        with patch.object(COVERAGE.subprocess, "run") as run:
            COVERAGE.run_tests(Path("build"), Path("profiles"))
            command = run.call_args.args[0]
            pattern = command[command.index("-L") + 1]
            for label in ("lab1-unit", "lab1-cli"):
                self.assertIsNotNone(re.fullmatch(pattern, label))
            for label in ("lab1-tools", "lab2", "lab2-ui", "lab2-cli"):
                self.assertIsNone(re.fullmatch(pattern, label))

    def test_threshold_is_strict_and_all_groups_must_be_present(self):
        def rows(covered):
            return [{"variant": variant, "count": 10, "covered": covered} for variant in COVERAGE.VARIANTS]
        with patch("builtins.print"):
            self.assertFalse(COVERAGE.enforce(rows(9))[1])
            self.assertTrue(COVERAGE.enforce(rows(10))[1])
            self.assertFalse(COVERAGE.enforce(rows(10)[:1])[1])


if __name__ == "__main__":
    unittest.main()
