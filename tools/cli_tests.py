#!/usr/bin/env python3
"""Black-box CLI regression tests; temporary files never touch user data."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest

PARSER = argparse.ArgumentParser()
PARSER.add_argument("--cube", required=True, type=Path)
PARSER.add_argument("--post", required=True, type=Path)
PARSER.add_argument("--examples", required=True, type=Path)
ARGS, REMAINING = PARSER.parse_known_args()


class CliTests(unittest.TestCase):
    scenarios = 0

    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory(prefix="ppois-cli-")
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)
        self.solved = ARGS.examples / "cube_solved.txt"
        self.machine = ARGS.examples / "post_mark_right.txt"
        self.existing = self.root / "existing.txt"
        self.existing.write_text("ORIGINAL", encoding="utf-8")

    def run_app(self, binary, lines=(), arguments=(), code=0):
        CliTests.scenarios += 1
        text = "\n".join(map(str, lines)) + ("\n" if lines else "")
        result = subprocess.run([str(binary), *map(str, arguments)], input=text,
                                text=True, capture_output=True, timeout=20)
        self.assertEqual(result.returncode, code, result.stdout + result.stderr)
        return result

    def cube(self, lines=(), arguments=(), code=0):
        return self.run_app(ARGS.cube, lines, arguments, code)

    def post(self, lines=(), arguments=(), code=0):
        return self.run_app(ARGS.post, lines, arguments, code)

    def test_cube_workflow(self):
        saved = self.root / "cube with spaces.txt"
        result = self.cube([1, 4, 2, 3, 1, 2, 3, 2, 3, 0, 3, 5,
                            7, 0, 8, 0, 8, 1, 7, 1, 5, saved, 0], [self.solved])
        self.assertEqual(saved.read_text(), self.solved.read_text())
        self.assertIn("Новая случайная игра", result.stdout)
        self.assertIn("Начата новая игра:", result.stdout)
        startup = self.root / "startup.txt"
        self.cube([5, startup, 0])
        self.assertEqual(len(startup.read_text().split()), 54)

    def test_cube_files_and_validation(self):
        saved = self.root / "saved.txt"
        result = self.cube(["abc", -1, 9, "1 x", "9223372036854775808", 5, "", saved,
                            5, saved, 0, 5, saved, 1, 6, "", saved, 0], [self.solved])
        self.assertIn("Введите целое число", result.stdout)
        self.assertIn("Путь не должен быть пустым", result.stdout)
        self.assertIn("Сохранение отменено", result.stdout)
        self.assertIn("Состояние загружено", result.stdout)
        self.assertEqual(saved.read_text(), self.solved.read_text())
        self.cube([5, self.existing, 0, 0])
        self.assertEqual(self.existing.read_text(), "ORIGINAL")

    def test_cube_bad_files(self):
        self.cube(arguments=[self.root / "missing"], code=1)
        self.cube(arguments=[self.solved, "extra"], code=1)
        for index, text in enumerate(["", "X", "W " * 54, self.solved.read_text() + "EXTRA"]):
            bad = self.root / "bad.txt"
            bad.write_text(text)
            self.cube(arguments=[bad], code=1)
            saved = self.root / f"preserved-{index}.txt"
            result = self.cube([6, bad, 5, saved, 0], [self.solved])
            self.assertTrue(result.stderr)
            self.assertEqual(saved.read_text(), self.solved.read_text())
        for path in [self.root, self.root / "missing" / "file"]:
            self.assertTrue(self.cube([5, path, 0]).stderr)
        self.assertTrue(self.cube([6, self.root / "missing", 0]).stderr)

    def test_cube_eof_at_every_prompt(self):
        cases = [[], [2], [2, 1], [3], [5], [5, self.existing], [6], [7], [8]]
        for lines in cases:
            with self.subTest(lines=lines):
                self.assertIn("Работа завершена", self.cube(lines).stdout)

    def test_post_file_modes(self):
        plain = self.post(arguments=[self.machine])
        self.assertIn("Выполнено команд: 3", plain.stdout)
        self.assertIn("HEAD 1", plain.stdout)
        self.assertNotIn("EXECUTED", plain.stdout)
        logged = self.post(arguments=[self.machine, "-log"])
        self.assertEqual(logged.stdout.count("EXECUTED "), 3)
        self.assertIn("EXECUTED 3\nHEAD 1\nMARKS 1 0\nCURRENT 3\nHALTED 1", logged.stdout)
        increment = self.post(arguments=[ARGS.examples / "post_increment.txt", "-log"])
        self.assertEqual(increment.stdout.count("EXECUTED "), 7)
        self.assertIn("MARKS 3 0 1 2", increment.stdout)
        self.assertIn("Использование", self.post(arguments=["--help"]).stdout)
        self.post(arguments=[self.machine, "--bad"], code=1)
        self.post(arguments=[self.machine, "-log", "extra"], code=1)
        self.post(arguments=[self.root / "missing"], code=1)
        self.post(arguments=[ARGS.examples / "post_loop.txt"], code=1)

    def test_post_workflow(self):
        saved = self.root / "machine with spaces.txt"
        result = self.post([1, 2, 3, 7, "HALT", 4, 7, "RIGHT 3", 5, 7, 5, 999,
                            6, 0, 1, 7, -1, 8, 9, 10, 1, 8, 9, 10, 0,
                            10, 0, 10, 1, 9, 10, 0, 12, "", saved, 11, "", saved,
                            10, 1, 9, 10, 0, 0])
        self.assertFalse(result.stderr, result.stderr)
        self.assertIn("Команда не найдена", result.stdout)
        self.assertIn("Машина уже остановлена", result.stdout)
        self.assertIn("Состояние загружено", result.stdout)
        self.assertIn("HALTED 1", saved.read_text())
        self.assertIn("Выполнено команд: 0", self.post(arguments=[saved]).stdout)

    def test_post_resumes_a_saved_intermediate_state(self):
        snapshot = self.root / "paused.txt"
        self.post([11, self.machine, 8, 12, snapshot, 0])
        self.assertIn("CURRENT 2\nHALTED 0", snapshot.read_text())
        resumed = self.post(arguments=[snapshot, "-log"])
        self.assertEqual(resumed.stdout.count("EXECUTED "), 2)
        self.assertIn("EXECUTED 2", resumed.stdout)
        self.assertNotIn("EXECUTED 1", resumed.stdout)
        self.assertIn("HEAD 1", resumed.stdout)

    def test_post_validation_and_overwrite(self):
        saved = self.root / "valid.txt"
        result = self.post(["abc", -1, 13, "1 x", "9223372036854775808",
                            3, 42, "HALT 3", "IF -1 2", "HALT", 3, 1, "HALT",
                            4, 999, "HALT", 12, saved, 12, saved, 0, 12, saved, 1,
                            12, self.existing, 0, 0])
        self.assertIn("Некорректная команда", result.stdout)
        self.assertIn("Введите целое число", result.stdout)
        self.assertIn("Сохранение отменено", result.stdout)
        self.assertTrue(result.stderr)
        self.assertEqual(self.existing.read_text(), "ORIGINAL")
        for path in [self.root, self.root / "missing" / "file"]:
            self.assertTrue(self.post([12, path, 0]).stderr)

    def test_post_bad_files_preserve_state(self):
        for index, text in enumerate(["", "HEAD x", self.machine.read_text() + "EXTRA",
                     self.machine.read_text().replace("CURRENT 1", "CURRENT 99")]):
            bad = self.root / "bad.txt"
            bad.write_text(text)
            self.post(arguments=[bad], code=1)
            before = self.root / f"before-{index}.txt"
            after = self.root / f"after-{index}.txt"
            result = self.post([12, before, 11, bad, 12, after, 0])
            self.assertTrue(result.stderr)
            self.assertEqual(before.read_text(), after.read_text())
        self.assertTrue(self.post([11, self.root / "missing", 0]).stderr)

    def test_post_execution_errors(self):
        for rules in ["1 MARK 99", "1 ERASE 1"]:
            bad = self.root / "execution.txt"
            bad.write_text(f"HEAD 0\nMARKS 0\nPROGRAM 1\n{rules}\nCURRENT 1\nHALTED 0\n")
            failed = self.post(arguments=[bad, "-log"], code=1)
            self.assertNotIn("EXECUTED", failed.stdout)
            self.assertTrue(self.post([11, bad, 8, 0]).stderr)
        loop = ARGS.examples / "post_loop.txt"
        self.assertTrue(self.post([11, loop, 9, 2, 0, 0]).stderr)
        empty = self.root / "empty.txt"
        empty.write_text("HEAD 0\nMARKS 0\nPROGRAM 0\nCURRENT 0\nHALTED 0\n")
        self.assertTrue(self.post([11, empty, 8, 0]).stderr)

    def test_post_eof_at_every_prompt(self):
        cases = [[], [3], [3, 7], [4], [4, 7], [5], [6], [6, 0], [7],
                 [9], [9, 5], [10], [11], [12], [12, self.existing]]
        for lines in cases:
            with self.subTest(lines=lines):
                self.assertIn("Работа завершена", self.post(lines).stdout)

    @classmethod
    def tearDownClass(cls):
        print(f"CLI subprocess scenarios: {cls.scenarios}")


if __name__ == "__main__":
    unittest.main(argv=[__file__, *REMAINING])
