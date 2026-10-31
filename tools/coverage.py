#!/usr/bin/env python3
"""Run fresh CTest profiles and enforce >90% line coverage of production code."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VARIANTS = ("rubiks_cube", "post_machine")


def llvm_tool(name, variable):
    configured = os.environ.get(variable)
    if configured:
        return configured
    found = shutil.which(name)
    if found:
        return found
    if shutil.which("xcrun"):
        return subprocess.check_output(["xcrun", "--find", name], text=True).strip()
    raise RuntimeError(f"Cannot find {name}; set {variable}")


def production_file(filename, unit_only=False):
    path = Path(filename).resolve()
    for variant in VARIANTS:
        base = ROOT / "lab1" / variant
        folders = ("src", "include") if unit_only else ("src", "app", "include")
        if any(path.is_relative_to(base / folder) for folder in folders):
            return variant
    return None


def summarize(data, unit_only=False):
    files = []
    for unit in data["data"]:
        for entry in unit["files"]:
            variant = production_file(entry["filename"], unit_only)
            if variant:
                files.append({"file": str(Path(entry["filename"]).resolve().relative_to(ROOT)),
                              "variant": variant, **entry["summary"]["lines"]})
    # llvm-cov merges the mappings of shared library code from all binaries.
    if len({entry["file"] for entry in files}) != len(files):
        raise RuntimeError("Duplicate source mappings in llvm-cov export")
    for variant in VARIANTS:
        expected = ROOT / "lab1" / variant
        folders = ("src",) if unit_only else ("src", "app")
        for source in [path for folder in folders for path in (expected / folder).glob("*.cpp")]:
            if str(source.relative_to(ROOT)) not in {entry["file"] for entry in files}:
                raise RuntimeError(f"Missing coverage mapping: {source}")
    return files


def enforce(files):
    groups = {}
    for group in (*VARIANTS, "total"):
        selected = [entry for entry in files if group == "total" or entry["variant"] == group]
        count = sum(entry["count"] for entry in selected)
        covered = sum(entry["covered"] for entry in selected)
        percent = 100 * covered / count if count else 0
        groups[group] = {"covered": covered, "count": count, "percent": percent}
        print(f"{group}: {covered}/{count} lines = {percent:.2f}%", flush=True)
    return groups, all(value["count"] > 0 and value["covered"] * 100 > value["count"] * 90
                       for value in groups.values())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--unit-only", action="store_true",
                        help="Run only Catch2 tests and measure src/include, without CLI code")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    output = build / ("unit-coverage" if args.unit_only else "coverage")
    output.mkdir(parents=True, exist_ok=True)
    profiles = Path(tempfile.mkdtemp(prefix="raw-", dir=output))
    environment = dict(os.environ, LLVM_PROFILE_FILE=str(profiles / "%p-%m.profraw"))
    test_command = ["ctest", "--test-dir", str(build), "--output-on-failure"]
    if args.unit_only:
        test_command.extend(["-E", "cli_integration"])
    subprocess.run(test_command, env=environment, check=True)
    raw = sorted(profiles.glob("*.profraw"))
    if not raw:
        raise RuntimeError("No profiles: configure with ENABLE_COVERAGE=ON")
    merged = output / "merged.profdata"
    subprocess.run([llvm_tool("llvm-profdata", "LLVM_PROFDATA"), "merge", "-sparse",
                    *map(str, raw), "-o", str(merged)], check=True)
    binaries = [build / "lab1" / name for name in ("lab1_tests", "rubiks_cube", "post_machine")]
    objects = [str(binaries[0]), *[f"-object={binary}" for binary in binaries[1:]],
               f"-instr-profile={merged}"]
    cov = llvm_tool("llvm-cov", "LLVM_COV")
    exported = subprocess.check_output([cov, "export", *objects], text=True)
    files = summarize(json.loads(exported), args.unit_only)
    groups, passed = enforce(files)
    scope = "unit tests: src/include" if args.unit_only else "unit and CLI tests: src/app/include"
    report = {"metric": "lines", "threshold": ">90%", "scope": scope, "groups": groups, "files": files}
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    subprocess.run([cov, "show", *objects, "-format=html", f"-output-dir={output / 'html'}",
                    *[str(ROOT / entry["file"]) for entry in files]], check=True)
    print(f"Reports: {output / 'report.json'} and {output / 'html' / 'index.html'}")
    if not passed:
        raise RuntimeError("Coverage must be strictly greater than 90% for each variant and total")


if __name__ == "__main__":
    main()
