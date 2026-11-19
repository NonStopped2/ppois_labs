#!/usr/bin/env python3
"""Measure lab4 production lines using only C++ Catch2 tests."""
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
    tool = shutil.which(name)
    if tool is None:
        raise FileNotFoundError(name)
    return tool


def summarize(report, directories):
    expected = set(directories[0].rglob("*.hpp"))
    expected.update(file for directory in directories for file in directory.rglob("*.cpp"))
    expected.discard(LAB / "app/main.cpp")  # The launcher is exercised by the packaged CLI test.
    lines_by_file = {}
    for unit in report["data"]:
        for entry in unit["files"]:
            path = Path(entry["filename"]).resolve()
            if path in expected:
                if path in lines_by_file:
                    raise ValueError(f"Duplicate line mapping: {path}")
                lines_by_file[path] = entry["summary"]["lines"]
    missing = expected - lines_by_file.keys()
    if missing:
        raise ValueError(f"Missing coverage mappings: {sorted(map(str, missing))}")
    covered = sum(lines["covered"] for lines in lines_by_file.values())
    total = sum(lines["count"] for lines in lines_by_file.values())
    if total == 0:
        raise ValueError("No executable lines measured")
    for path, lines in sorted(lines_by_file.items()):
        print(f"{str(path.relative_to(LAB)):35} {lines['covered']}/{lines['count']}")
    print(f"Lab4 C++ unit line coverage: {covered}/{total} = {100 * covered / total:.2f}%")
    return covered * 100 > total * 90


def collect_report(args):
    build = args.build_dir.resolve()
    output = Path(tempfile.mkdtemp(prefix="lab4-unit-coverage-", dir=build))
    binary = build / "lab4/lab4_tests"
    if not binary.is_file():
        raise FileNotFoundError(f"Build {binary.name} first")
    label = "^lab4-(unit|ui)$" if args.with_ui else "^lab4-unit$"
    environment = dict(os.environ, LLVM_PROFILE_FILE=str(output / "%p-%m.profraw"))
    subprocess.run(["ctest", "--test-dir", str(build), "-L", label, "--no-tests=error",
                    "--output-on-failure"], env=environment, check=True)
    profiles = sorted(output.glob("*.profraw"))
    if not profiles:
        raise ValueError("No profiles: configure LAB4_ENABLE_COVERAGE=ON")
    merged = output / "merged.profdata"
    subprocess.run([args.llvm_profdata or llvm_tool("llvm-profdata"), "merge", "-sparse",
                    *map(str, profiles), "-o", str(merged)], check=True)
    result = subprocess.check_output([
        args.llvm_cov or llvm_tool("llvm-cov"), "export", str(binary), f"-instr-profile={merged}",
        "-summary-only",
    ], text=True)
    report = output / "coverage.json"
    report.write_text(result, encoding="utf-8")
    print(f"Report: {report}")
    return json.loads(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--with-ui", action="store_true")
    parser.add_argument("--llvm-cov")
    parser.add_argument("--llvm-profdata")
    args = parser.parse_args()
    directories = [LAB / "include"]
    if args.with_ui:
        directories.append(LAB / "app")
    return 0 if summarize(collect_report(args), directories) else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        sys.exit(f"Lab4 coverage error: {error}")
