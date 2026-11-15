#!/usr/bin/env python3
"""Collect only lab3 C++ unit profiles and enforce >90% production line coverage."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

LAB = Path(__file__).resolve().parents[1]


def llvm_tool(name):
    if sys.platform == "darwin":
        return subprocess.check_output(["xcrun", "--find", name], text=True).strip()
    executable = shutil.which(name)
    if executable is None:
        raise FileNotFoundError(f"Missing {name}")
    return executable


def summarize(data, expected, include_ui=False, scope="core"):
    folders = [LAB / "persistence"] if scope == "persistence" else [LAB / "src", LAB / "include"]
    if include_ui:
        folders.append(LAB / "app")
    files = {}
    for unit in data["data"]:
        for entry in unit["files"]:
            path = Path(entry["filename"]).resolve()
            if not any(path.is_relative_to(folder) for folder in folders):
                continue
            if path in files:
                raise ValueError(f"Duplicate source mapping: {path}")
            files[path] = entry["summary"]["lines"]
    missing = set(expected) - files.keys()
    if missing:
        raise ValueError(f"Missing production source mappings: {sorted(map(str, missing))}")
    total = sum(item["count"] for item in files.values())
    covered = sum(item["covered"] for item in files.values())
    if total == 0:
        raise ValueError("No production lines were measured")
    for path, lines in sorted(files.items()):
        print(f"{str(path.relative_to(LAB)):60} {lines['covered']}/{lines['count']}")
    print(f"Lab3 {scope} C++ unit line coverage: {covered}/{total} = {100 * covered / total:.2f}%")
    return covered * 100 > total * 90


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--with-ui", action="store_true")
    parser.add_argument("--scope", choices=("core", "persistence"), default="core")
    parser.add_argument("--llvm-cov")
    parser.add_argument("--llvm-profdata")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    if args.with_ui and args.scope != "core":
        parser.error("--with-ui requires --scope core")
    binaries = [build / ("lab3/shop_persistence_tests" if args.scope == "persistence" else "lab3/shop_core_tests")]
    if args.with_ui:
        binaries += [build / "lab3/shop_ui_tests", build / "lab3/shop_cli"]
    for binary in binaries:
        if not binary.is_file():
            raise FileNotFoundError(f"Build {binary.name} first")
    output = Path(tempfile.mkdtemp(prefix="lab3-unit-coverage-", dir=build))
    environment = dict(os.environ, LLVM_PROFILE_FILE=str(output / "%p-%m.profraw"))
    selection = "^lab3-persistence$" if args.scope == "persistence" else "^lab3-unit$"
    if args.with_ui:
        selection = "^lab3-(unit|ui)$"
    subprocess.run(["ctest", "--test-dir", str(build), "-L", selection, "--no-tests=error",
                    "--output-on-failure"], check=True, env=environment)
    profiles = sorted(output.glob("*.profraw"))
    if not profiles:
        raise ValueError("No profiles: configure LAB3_ENABLE_COVERAGE=ON")
    merged = output / "merged.profdata"
    subprocess.run([args.llvm_profdata or llvm_tool("llvm-profdata"), "merge", "-sparse",
                    *map(str, profiles), "-o", str(merged)], check=True)
    command = [args.llvm_cov or llvm_tool("llvm-cov"), "export", str(binaries[0]),
               *[f"-object={binary}" for binary in binaries[1:]], f"-instr-profile={merged}", "-summary-only"]
    result = subprocess.check_output(command, text=True)
    (output / "coverage.json").write_text(result, encoding="utf-8")
    folders = [LAB / "persistence"] if args.scope == "persistence" else [LAB / "src"]
    folders += [LAB / "app"] if args.with_ui else []
    expected = [path.resolve() for folder in folders for path in folder.rglob("*.cpp")]
    passed = summarize(json.loads(result), expected, args.with_ui, args.scope)
    print(f"Report: {output / 'coverage.json'}")
    return 0 if passed else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Lab3 coverage error: {error}", file=sys.stderr)
        sys.exit(1)
