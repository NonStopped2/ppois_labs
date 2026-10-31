#!/usr/bin/env python3
"""Check pinned C++ formatting, Python syntax and production-code static analysis."""
import ast
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    local = ROOT / "build" / "tools" / "bin" / "clang-format"
    formatter = os.environ.get("CLANG_FORMAT") or (str(local) if local.exists() else shutil.which("clang-format"))
    analyzer = os.environ.get("CPPCHECK") or shutil.which("cppcheck")
    if not formatter or not analyzer:
        raise RuntimeError("Install clang-format 18.1.8 and cppcheck; see lab1/README.md")
    version = subprocess.check_output([formatter, "--version"], text=True)
    if "18.1.8" not in version:
        raise RuntimeError(f"Expected clang-format 18.1.8, got {version.strip()}")
    sources = []
    for variant in ("rubiks_cube", "post_machine"):
        base = ROOT / "lab1" / variant
        for folder in ("include", "src", "app", "tests"):
            sources.extend(sorted((base / folder).glob("*.cpp")))
            sources.extend(sorted((base / folder).glob("*.hpp")))
    subprocess.run([formatter, "--dry-run", "--Werror", *map(str, sources)], check=True, cwd=ROOT)
    for source in (ROOT / "tools").glob("*.py"):
        ast.parse(source.read_text(encoding="utf-8"), filename=str(source))
    subprocess.run([analyzer, "--std=c++20", "--enable=warning,performance,portability",
                    "--error-exitcode=1", "--suppress=missingIncludeSystem", "--quiet",
                    "-I", "lab1/rubiks_cube/include", "-I", "lab1/post_machine/include",
                    "lab1/rubiks_cube/src", "lab1/rubiks_cube/app",
                    "lab1/post_machine/src", "lab1/post_machine/app"], check=True, cwd=ROOT)
    print("Formatting, Python syntax and cppcheck: passed")


if __name__ == "__main__":
    main()
