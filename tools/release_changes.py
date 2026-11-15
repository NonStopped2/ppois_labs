#!/usr/bin/env python3
"""Select release scopes from the complete before..after diff of a main push."""
import os
from pathlib import Path
import re
import subprocess


LABS = frozenset({"lab1", "lab2"})
SHARED_FILES = frozenset({
    "CMakeLists.txt", "CMakePresets.json", ".clang-format", ".clang-tidy", ".gitmodules",
    "tools/release_changes.py", "tools/test_release_changes.py", "tools/test_release_workflow.py",
})


def affected_labs(paths):
    """Root tools belong to lab1 except the explicitly shared release helpers."""
    affected = set()
    for path in paths:
        if path in SHARED_FILES or path.startswith(("cmake/", ".github/actions/")):
            affected.update(LABS)
        elif path.startswith("lab1/") or path in {"Doxyfile.in", ".github/workflows/lab1.yml"}:
            affected.add("lab1")
        elif path.startswith("lab2/") or path == ".github/workflows/lab2.yml":
            affected.add("lab2")
        elif path.startswith("tools/"):
            affected.add("lab1")
        elif path.startswith(".github/workflows/"):
            affected.update(LABS)
    return affected


def detect_changed_labs(event_name, ref, before, after, repo="."):
    """Manual tags bypass path filtering in YAML; PRs never authorize a release."""
    if event_name != "push" or ref != "refs/heads/main":
        return set()
    if not all(re.fullmatch(r"[0-9a-f]{40}", sha) for sha in (before, after)):
        raise ValueError("A main push must supply full before and after commit SHAs")
    if before == "0" * 40:
        # A newly created main has no previous tree: consider all current files.
        arguments = ["ls-tree", "-r", "--name-only", "-z", after]
    else:
        # No rename detection: moving between labs affects both source and destination.
        arguments = ["diff", "--name-only", "--no-renames", "-z", before, after, "--"]
    result = subprocess.run(
        ["git", "-C", str(repo), *arguments], check=True, capture_output=True,
    )
    paths = [os.fsdecode(path) for path in result.stdout.split(b"\0") if path]
    return affected_labs(paths)


def main():
    affected = detect_changed_labs(
        os.environ["EVENT_NAME"], os.environ["RELEASE_REF"],
        os.environ["BEFORE_SHA"], os.environ["RELEASE_SHA"],
    )
    output = "".join(f"{lab}={str(lab in affected).lower()}\n" for lab in sorted(LABS))
    print(output, end="")
    if os.environ.get("GITHUB_OUTPUT"):
        with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as stream:
            stream.write(output)


if __name__ == "__main__":
    main()
