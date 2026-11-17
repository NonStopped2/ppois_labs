#!/usr/bin/env python3
"""Check lab4 formatting, header self-containment, Python syntax and C++ quality."""
import argparse
import ast
import os
from pathlib import Path
import re
import subprocess

LAB = Path(__file__).resolve().parents[1]
FORMAT_VERSION = "18.1.8"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    local = LAB.parent / "build/tools/bin/clang-format"
    parser.add_argument("--clang-format", default=str(local) if local.exists() else "clang-format")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "clang++"))
    args = parser.parse_args()
    version = subprocess.check_output([args.clang_format, "--version"], text=True)
    if not re.search(rf"\bversion {re.escape(FORMAT_VERSION)}(?=\s|$)", version):
        parser.error(f"Expected clang-format {FORMAT_VERSION}, got {version.strip()}")
    folders = (LAB / "include", LAB / "app", LAB / "tests")
    headers = sorted(path for folder in folders[:2] for path in folder.rglob("*.hpp"))
    sources = sorted(path for folder in folders for path in folder.rglob("*")
                     if path.suffix in (".cpp", ".hpp"))
    subprocess.run([args.clang_format, "--dry-run", "--Werror", *map(str, sources)], check=True)
    for source in (LAB / "tools").glob("*.py"):
        ast.parse(source.read_text(encoding="utf-8"), filename=str(source))
    # A single command table uses the same callback signature for read and write operations.
    subprocess.run(["cppcheck", "--std=c++20", "--enable=warning,style,performance,portability",
                    "--error-exitcode=1", "--inline-suppr", "--suppress=missingIncludeSystem",
                    f"--suppress=constParameterCallback:{LAB / 'app/Console.cpp'}", "--quiet",
                    "-I", str(LAB / "include"), str(LAB / "app")], check=True)
    for header in headers:
        subprocess.run([args.cxx, "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                        "-I", str(LAB / "include"), "-x", "c++", "-fsyntax-only", "-"],
                       input=f'#include "{header}"\n', text=True, check=True)
    print(f"Lab4 quality passed; {len(headers)} standalone headers")


if __name__ == "__main__":
    main()
