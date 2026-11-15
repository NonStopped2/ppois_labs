"""Exercise release shell steps with a fake gh; never create tags or releases."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SHA = "a" * 40


def shell_step(workflow, name):
    """Extract a literal run block; actionlint separately validates YAML/expressions."""
    text = workflow.read_text(encoding="utf-8")
    section = text.split(f"      - name: {name}\n", 1)[1]
    block = section.split("        run: |\n", 1)[1]
    lines = []
    for line in block.splitlines():
        if line and not line.startswith("          "):
            break
        lines.append(line[10:] if line else "")
    return "\n".join(lines) + "\n"


class ReleaseWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workflows = sorted((ROOT / ".github/workflows").glob("lab[12].yml"))
        if not cls.workflows:
            raise AssertionError("No laboratory workflows found")

    def select_tag(self, workflow, ref, ref_name, sha=SHA):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "output"
            environment = dict(os.environ, RELEASE_REF=ref, RELEASE_REF_NAME=ref_name,
                               RELEASE_SHA=sha, GITHUB_OUTPUT=str(output))
            result = subprocess.run(
                ["bash", "-e", "-o", "pipefail", "-c", shell_step(workflow, "Select release tag")],
                env=environment, capture_output=True, text=True, check=False,
            )
            return result, output.read_text() if output.exists() else ""

    def publish_arguments(self, workflow, ref, tag):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "dist").mkdir()
            for name in ("linux", "macos", "docs"):
                (root / "dist" / f"{name}.tar.gz").touch()
                (root / "dist" / f"{name}.tar.gz.sha256").touch()
            fake = root / "gh"
            fake.write_text(
                "#!/usr/bin/env python3\n"
                "import json, os, pathlib, sys\n"
                "pathlib.Path(os.environ['GH_ARGS_LOG']).write_text(json.dumps(sys.argv[1:]))\n",
                encoding="utf-8",
            )
            fake.chmod(0o755)
            log = root / "arguments.json"
            environment = dict(os.environ, PATH=str(root) + os.pathsep + os.environ["PATH"],
                               GH_ARGS_LOG=str(log), RELEASE_REF=ref, RELEASE_SHA=SHA,
                               RELEASE_TAG=tag, GH_REPO="test/repository", GH_TOKEN="fake")
            subprocess.run(
                ["bash", "-e", "-o", "pipefail", "-c",
                 shell_step(workflow, "Publish release without moving existing tags")],
                cwd=root, env=environment, check=True, capture_output=True, text=True,
            )
            return json.loads(log.read_text())

    def test_release_requires_successful_checks_and_main_push_or_manual_tag(self):
        for workflow in self.workflows:
            with self.subTest(workflow=workflow.name):
                release = workflow.read_text().split("\n  release:\n", 1)[1]
                self.assertIn("    needs: [changes, build, quality]\n", release)
                condition = re.search(r"    if: >-\n(.*?)    runs-on:", release, re.S)
                self.assertIsNotNone(condition)
                expected = ("github.event_name == 'push' && "
                            f"((github.ref == 'refs/heads/main' && needs.changes.outputs.{workflow.stem} == 'true') || "
                            f"startsWith(github.ref, 'refs/tags/{workflow.stem}-v'))")
                self.assertEqual(" ".join(condition.group(1).split()), expected)

    def test_scope_job_gets_full_history_and_push_endpoints_without_skipping_checks(self):
        for workflow in self.workflows:
            with self.subTest(workflow=workflow.name):
                text = workflow.read_text()
                scope = text.split("\n  changes:\n", 1)[1].split("\n  build:\n", 1)[0]
                self.assertIn("fetch-depth: 0", scope)
                self.assertIn("BEFORE_SHA: ${{ github.event.before }}", scope)
                self.assertIn("RELEASE_SHA: ${{ github.sha }}", scope)
                self.assertIn("EVENT_NAME: ${{ github.event_name }}", scope)
                self.assertIn("RELEASE_REF: ${{ github.ref }}", scope)
                self.assertIn("run: python3 tools/release_changes.py", scope)
                for lab in ("lab1", "lab2"):
                    self.assertIn(f"{lab}: ${{{{ steps.scope.outputs.{lab} }}}}", scope)
                self.assertNotRegex(scope, r"(?m)^    if:")
                self.assertNotRegex(text, r"(?m)^    paths(?:-ignore)?:")
                for job, following in (("build", "quality"), ("quality", "release")):
                    section = text.split(f"\n  {job}:\n", 1)[1].split(f"\n  {following}:\n", 1)[0]
                    self.assertNotRegex(section, r"(?m)^    (?:if|needs):")

    def test_release_shell_steps_are_valid_bash(self):
        for workflow in self.workflows:
            for name in ("Select release tag", "Verify release assets",
                         "Publish release without moving existing tags"):
                with self.subTest(workflow=workflow.name, step=name):
                    subprocess.run(["bash", "-n"], input=shell_step(workflow, name),
                                   text=True, check=True, capture_output=True)

    def test_main_tag_is_bound_to_full_verified_commit(self):
        for workflow in self.workflows:
            with self.subTest(workflow=workflow.name):
                result, output = self.select_tag(workflow, "refs/heads/main", "main")
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(output, f"tag={workflow.stem}-main-{SHA}\n")
                result, _ = self.select_tag(workflow, "refs/heads/main", "main", "invalid")
                self.assertNotEqual(result.returncode, 0)

    def test_manual_tags_are_validated_and_feature_branches_rejected(self):
        for workflow in self.workflows:
            lab = workflow.stem
            with self.subTest(workflow=workflow.name):
                tag = f"{lab}-v2.3.4"
                result, output = self.select_tag(workflow, f"refs/tags/{tag}", tag)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(output, f"tag={tag}\n")
                for bad in (f"{lab}-v1.2", f"{lab}-v1.2.3-extra", "other-v1.2.3"):
                    result, _ = self.select_tag(workflow, f"refs/tags/{bad}", bad)
                    self.assertNotEqual(result.returncode, 0)
                result, _ = self.select_tag(workflow, f"refs/heads/feature/{lab}", f"feature/{lab}")
                self.assertNotEqual(result.returncode, 0)

    def test_main_publication_is_stable_and_targets_verified_sha(self):
        for workflow in self.workflows:
            with self.subTest(workflow=workflow.name):
                tag = f"{workflow.stem}-main-{SHA}"
                arguments = self.publish_arguments(workflow, "refs/heads/main", tag)
                self.assertEqual(arguments[:3], ["release", "create", tag])
                self.assertEqual(arguments[arguments.index("--target") + 1], SHA)
                self.assertNotIn("--prerelease", arguments)
                self.assertNotIn("--draft", arguments)
                self.assertNotIn("--verify-tag", arguments)
                self.assertEqual(sum(arg.endswith(".tar.gz") for arg in arguments), 3)
                self.assertEqual(sum(arg.endswith(".sha256") for arg in arguments), 3 if workflow.stem == "lab2" else 0)

    def test_manual_publication_preserves_existing_tag_mode(self):
        for workflow in self.workflows:
            with self.subTest(workflow=workflow.name):
                tag = f"{workflow.stem}-v2.3.4"
                arguments = self.publish_arguments(workflow, f"refs/tags/{tag}", tag)
                self.assertIn("--verify-tag", arguments)
                self.assertNotIn("--target", arguments)
                self.assertEqual("--prerelease" in arguments, workflow.stem == "lab2")


if __name__ == "__main__":
    unittest.main()
