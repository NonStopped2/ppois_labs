#!/usr/bin/env python3
"""Check lab2 formatting, Python syntax, production cppcheck and standalone headers."""
import argparse
import ast
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

LAB_ROOT = Path(__file__).resolve().parents[1]
ROOT = LAB_ROOT.parent
FORMAT_VERSION = "18.1.8"


def cpp_sources():
    return sorted(path for folder in ("include", "src", "app", "tests")
                  for path in (LAB_ROOT / folder).rglob("*") if path.suffix in (".hpp", ".cpp"))


def check_formatter(executable):
    version = subprocess.check_output([executable, "--version"], text=True).strip()
    if not re.search(r"\bversion " + re.escape(FORMAT_VERSION) + r"(?=\s|$)", version):
        raise ValueError(f"Expected clang-format {FORMAT_VERSION}, got {version}")


def check_headers(compiler):
    headers = sorted((LAB_ROOT / "include").rglob("*.hpp")) + sorted((LAB_ROOT / "app").glob("*.hpp"))
    command = [compiler, "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
               "-I", str(LAB_ROOT / "include"), "-I", str(LAB_ROOT / "app"), "-x", "c++", "-fsyntax-only", "-"]
    for header in headers:
        # Каждый заголовок проверяется отдельно, без помощи случайных транзитивных include.
        subprocess.run(command, input=f'#include "{header}"\n', text=True, check=True, cwd=ROOT)
    print(f"Standalone headers: {len(headers)} passed", flush=True)


def executable(value, name):
    result = shutil.which(value or name)
    if result is None:
        raise FileNotFoundError(f"Cannot find {value or name}; see lab2/CI.md")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    local = ROOT / "build/tools/bin/clang-format"
    parser.add_argument("--clang-format", default=os.environ.get("CLANG_FORMAT") or (str(local) if local.exists() else None))
    parser.add_argument("--cppcheck", default=os.environ.get("CPPCHECK"))
    parser.add_argument("--cxx", default=os.environ.get("CXX"))
    args = parser.parse_args()
    formatter = executable(args.clang_format, "clang-format")
    analyzer = executable(args.cppcheck, "cppcheck")
    compiler = executable(args.cxx, "clang++")
    check_formatter(formatter)
    subprocess.run([formatter, "--dry-run", "--Werror", *map(str, cpp_sources())], check=True, cwd=ROOT)
    for source in sorted((LAB_ROOT / "tools").glob("*.py")):
        ast.parse(source.read_text(encoding="utf-8"), filename=str(source))
    subprocess.run([analyzer, "--std=c++20", "--enable=warning,style,performance,portability",
                    "--error-exitcode=1", "--inline-suppr", "--suppress=missingIncludeSystem", "--quiet",
                    "-I", str(LAB_ROOT / "include"), "-I", str(LAB_ROOT / "app"),
                    str(LAB_ROOT / "src"), str(LAB_ROOT / "app")], check=True, cwd=ROOT)
    check_headers(compiler)
    print("Lab2 formatting, Python syntax and cppcheck: passed", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, SyntaxError, subprocess.CalledProcessError) as error:
        print(f"Quality error: {error}", file=sys.stderr)
        sys.exit(1)
