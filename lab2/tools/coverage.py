#!/usr/bin/env python3
"""Run lab2 unit tests and enforce strictly more than 90% line coverage."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


LAB_ROOT = Path(__file__).resolve().parents[1]


def llvm_tool(name: str) -> str:
    if sys.platform == "darwin":
        result = subprocess.run(["xcrun", "--find", name], check=True, capture_output=True, text=True)
        return result.stdout.strip()
    executable = shutil.which(name)
    if executable is None:
        raise FileNotFoundError(f"Cannot find {name}; provide its path explicitly")
    return executable


def run_tests(build_dir: Path, profile_dir: Path) -> None:
    environment = os.environ.copy()
    # Новый каталог исключает примесь профилей старого кода и других лабораторных.
    environment["LLVM_PROFILE_FILE"] = str(profile_dir / "unit-%p.profraw")
    subprocess.run(
        ["ctest", "--test-dir", str(build_dir), "-L", "^lab2$", "--output-on-failure"],
        check=True,
        env=environment,
    )


def merge_profiles(profile_dir: Path, tool: str) -> Path:
    profiles = sorted(profile_dir.glob("unit-*.profraw"))
    if not profiles:
        raise ValueError("No profiles: configure with -DLAB2_ENABLE_COVERAGE=ON and build tests")
    merged = profile_dir / "unit.profdata"
    subprocess.run([tool, "merge", "-sparse", *map(str, profiles), "-o", str(merged)], check=True)
    return merged


def export_coverage(executable: Path, profile: Path, tool: str) -> dict:
    sources = sorted((LAB_ROOT / "src").rglob("*.cpp"))
    sources.extend(sorted((LAB_ROOT / "include").rglob("*.hpp")))
    command = [tool, "export", str(executable), "-summary-only", f"-instr-profile={profile}"]
    result = subprocess.run(command + list(map(str, sources)), check=True, capture_output=True, text=True)
    (profile.parent / "coverage.json").write_text(result.stdout, encoding="utf-8")
    return json.loads(result.stdout)


def production_file(filename: str) -> bool:
    path = Path(filename).resolve()
    return path.is_relative_to(LAB_ROOT / "src") or path.is_relative_to(LAB_ROOT / "include")


def report_coverage(data: dict) -> bool:
    covered = total = 0
    for file_data in data["data"][0]["files"]:
        if not production_file(file_data["filename"]):
            continue
        lines = file_data["summary"]["lines"]
        covered += lines["covered"]
        total += lines["count"]
        relative = Path(file_data["filename"]).resolve().relative_to(LAB_ROOT)
        print(f"{str(relative):55} {lines['covered']:4}/{lines['count']}")
    if total == 0:
        raise ValueError("No production lines found; coverage cannot pass")
    print(f"Unit-only lab2 line coverage: {covered}/{total} = {100 * covered / total:.2f}%")
    # Целочисленное сравнение не превращает округлённые 90.00% в успешную проверку.
    return covered * 10 > total * 9


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--llvm-cov", help="Path to llvm-cov matching the compiler")
    parser.add_argument("--llvm-profdata", help="Path to llvm-profdata matching the compiler")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    build_dir = arguments.build_dir.resolve()
    executable = build_dir / "lab2" / "post_core_tests"
    if not executable.is_file():
        raise FileNotFoundError(f"Build lab2 tests first: missing {executable}")
    cov = arguments.llvm_cov or llvm_tool("llvm-cov")
    profdata = arguments.llvm_profdata or llvm_tool("llvm-profdata")
    profile_dir = Path(tempfile.mkdtemp(prefix="lab2-unit-coverage-", dir=build_dir))
    run_tests(build_dir, profile_dir)
    merged = merge_profiles(profile_dir, profdata)
    data = export_coverage(executable, merged, cov)
    print(f"Coverage artifacts: {profile_dir}")
    return 0 if report_coverage(data) else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Coverage error: {error}", file=sys.stderr)
        sys.exit(1)
