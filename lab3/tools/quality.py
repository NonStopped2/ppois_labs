#!/usr/bin/env python3
"""Check online-store formatting, Python syntax, cppcheck and header self-containment."""
import argparse
import ast
import os
from pathlib import Path
import re
import shutil
import subprocess

LAB = Path(__file__).resolve().parents[1]
ROOT = LAB.parent
FORMAT_VERSION = "18.1.8"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    local = ROOT / "build/tools/bin/clang-format"
    parser.add_argument("--clang-format", default=str(local) if local.exists() else "clang-format")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "clang++"))
    args = parser.parse_args()
    version = subprocess.check_output([args.clang_format, "--version"], text=True)
    if not re.search(r"\bversion " + re.escape(FORMAT_VERSION) + r"(?=\s|$)", version):
        raise ValueError(f"Expected clang-format {FORMAT_VERSION}, got {version.strip()}")
    folders = [LAB / name for name in ("include", "src", "app", "persistence", "tests")]
    sources = sorted(path for folder in folders for path in folder.rglob("*")
                     if path.suffix in (".cpp", ".hpp"))
    subprocess.run([args.clang_format, "--dry-run", "--Werror", *map(str, sources)], check=True)
    for source in (LAB / "tools").glob("*.py"):
        ast.parse(source.read_text(encoding="utf-8"), filename=str(source))
    analyzer = shutil.which("cppcheck")
    if analyzer is None:
        raise FileNotFoundError("cppcheck is required")
    production = [folder for folder in folders[:-1] if folder.exists() and folder.name != "include"]
    subprocess.run([analyzer, "--std=c++20", "--enable=warning,style,performance,portability",
                    "--error-exitcode=1", "--inline-suppr", "--suppress=missingIncludeSystem", "--quiet",
                    "-I", str(LAB / "include"), *map(str, production)], check=True)
    headers = sorted((LAB / "include").rglob("*.hpp"))
    for header in headers:
        subprocess.run([args.cxx, "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                        "-I", str(LAB / "include"), "-x", "c++", "-fsyntax-only", "-"],
                       input=f'#include "{header}"\n', text=True, check=True)
    print(f"Lab3 quality passed; {len(headers)} standalone headers")


if __name__ == "__main__":
    main()
