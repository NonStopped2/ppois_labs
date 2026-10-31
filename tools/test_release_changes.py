"""Test path policy and real Git diffs in isolated temporary repositories."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import release_changes


class ReleaseScopeTests(unittest.TestCase):
    def test_lab_paths_and_private_infrastructure(self):
        for path, expected in (
            ("lab1/rubiks_cube/src/RubiksCube.cpp", {"lab1"}),
            ("lab1/README.md", {"lab1"}),
            ("Doxyfile.in", {"lab1"}),
            ("tools/package.py", {"lab1"}),
            ("tools/cli_tests.py", {"lab1"}),
            (".github/workflows/lab1.yml", {"lab1"}),
            ("lab2/src/core/Money.cpp", {"lab2"}),
            ("lab2/tools/package.py", {"lab2"}),
            (".github/workflows/lab2.yml", {"lab2"}),
        ):
            with self.subTest(path=path):
                self.assertEqual(release_changes.affected_labs([path]), expected)

    def test_shared_build_and_release_infrastructure_affects_both(self):
        for path in (*release_changes.SHARED_FILES, "cmake/dependencies.cmake",
                     ".github/actions/setup/action.yml", ".github/workflows/shared.yml"):
            with self.subTest(path=path):
                self.assertEqual(release_changes.affected_labs([path]), {"lab1", "lab2"})

    def test_unrelated_changes_do_not_publish_either_lab(self):
        self.assertEqual(release_changes.affected_labs([
            "README.md", "AGENTS.md", ".gitignore", "lab3/README.md", "lab4/main.cpp",
        ]), set())
        self.assertEqual(release_changes.affected_labs([]), set())

    def test_tags_feature_pushes_prs_and_manual_runs_do_not_use_git(self):
        with patch.object(release_changes.subprocess, "run") as run:
            for event, ref in (("push", "refs/tags/lab1-v1.2.3"),
                               ("push", "refs/heads/feature/lab2"),
                               ("pull_request", "refs/pull/1/merge"),
                               ("workflow_dispatch", "refs/heads/main")):
                self.assertEqual(release_changes.detect_changed_labs(event, ref, "", ""), set())
            run.assert_not_called()

    @staticmethod
    def git(root, *arguments):
        return subprocess.run(
            ["git", "-C", str(root), "-c", "user.name=Release test", "-c",
             "user.email=release-test@example.invalid", "-c", "commit.gpgsign=false",
             "-c", "core.hooksPath=/dev/null", *arguments],
            check=True, capture_output=True, text=True,
        ).stdout.strip()

    def commit_file(self, root, path, message):
        target = root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(message, encoding="utf-8")
        self.git(root, "add", "--", path)
        self.git(root, "commit", "-qm", message)
        return self.git(root, "rev-parse", "HEAD")

    def detect(self, root, before, after):
        return release_changes.detect_changed_labs("push", "refs/heads/main", before, after, root)

    def test_all_commits_in_push_are_included_not_just_last_commit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.git(root, "init", "-q", "--initial-branch=main")
            before = self.commit_file(root, "README.md", "base")
            middle = self.commit_file(root, "lab1/first.cpp", "first lab")
            after = self.commit_file(root, "lab2/last.cpp", "second lab")
            self.assertEqual(self.detect(root, middle, after), {"lab2"})
            self.assertEqual(self.detect(root, before, after), {"lab1", "lab2"})
            self.assertEqual(self.detect(root, after, after), set())
            self.assertEqual(self.detect(root, "0" * 40, after), {"lab1", "lab2"})
            output = root / "outputs"
            with patch.dict(os.environ, EVENT_NAME="push", RELEASE_REF="refs/heads/main",
                            BEFORE_SHA=before, RELEASE_SHA=after, GITHUB_OUTPUT=str(output)):
                with patch.object(release_changes, "detect_changed_labs", return_value={"lab2"}):
                    release_changes.main()
            self.assertEqual(output.read_text(), "lab1=false\nlab2=true\n")

    def test_merge_and_squash_include_complete_topic_changes(self):
        for mode in ("--no-ff", "--squash"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                self.git(root, "init", "-q", "--initial-branch=main")
                before = self.commit_file(root, "README.md", "base")
                self.git(root, "checkout", "-qb", "topic")
                self.commit_file(root, "lab1/first.cpp", "first change")
                self.commit_file(root, "lab2/last.cpp", "last change")
                self.git(root, "checkout", "-q", "main")
                self.git(root, "merge", mode, "topic", "-m", "merge topic")
                if mode == "--squash":
                    self.git(root, "commit", "-qm", "squash topic")
                after = self.git(root, "rev-parse", "HEAD")
                self.assertEqual(self.detect(root, before, after), {"lab1", "lab2"})

    def test_renames_deletions_and_newlines_in_filenames(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.git(root, "init", "-q", "--initial-branch=main")
            before = self.commit_file(root, "lab1/file\nname.cpp", "original")
            (root / "lab2").mkdir()
            self.git(root, "mv", "lab1/file\nname.cpp", "lab2/file.cpp")
            self.git(root, "commit", "-qm", "move between labs")
            after = self.git(root, "rev-parse", "HEAD")
            self.assertEqual(self.detect(root, before, after), {"lab1", "lab2"})
            self.git(root, "rm", "lab2/file.cpp")
            self.git(root, "commit", "-qm", "delete tracked file")
            deleted = self.git(root, "rev-parse", "HEAD")
            self.assertEqual(self.detect(root, after, deleted), {"lab2"})

    def test_invalid_or_missing_base_fails_instead_of_silently_skipping_release(self):
        with self.assertRaises(ValueError):
            self.detect(".", "invalid", "a" * 40)
        with tempfile.TemporaryDirectory() as directory:
            self.git(directory, "init", "-q")
            with self.assertRaises(subprocess.CalledProcessError):
                self.detect(directory, "a" * 40, "b" * 40)


if __name__ == "__main__":
    unittest.main()
