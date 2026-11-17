#!/usr/bin/env python3
"""Install, relocate, test and archive lab3; independently verify downloaded native packages."""
import argparse
import hashlib
import hmac
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

LAB = Path(__file__).resolve().parents[1]
SHARE = Path("share/ppois_lab3")
GUIDES = ("README.md", "docs/CLI.md", "docs/DOMAIN.md", "docs/CLASS_CATALOG.md", "docs/REPORTS.md",
          "docs/SNAPSHOTS.md", "docs/BEHAVIORS.md", "docs/CI.md")
EXAMPLES = ("purchase.input", "coupon_cancel.input", "new_product.input")


def package_name(docs=False):
    if docs:
        return "ppois-lab3-docs"
    system = {"Darwin": "macos", "Linux": "linux"}.get(platform.system())
    if system is None:
        raise ValueError("Native packaging supports Linux and macOS")
    return f"ppois-lab3-{system}-{platform.machine().lower()}"


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def verify_checksum(archive):
    fields = archive.with_name(archive.name + ".sha256").read_text(encoding="utf-8").split()
    if (len(fields) != 2 or fields[1] != archive.name or not re.fullmatch(r"[0-9a-f]{64}", fields[0])
            or not hmac.compare_digest(fields[0], digest(archive))):
        raise ValueError(f"Checksum mismatch: {archive}")


def validate_members(members, destination):
    if not members or len(members) > 100000:
        raise ValueError("Empty or oversized archive")
    names = set()
    total = 0
    for member in members:
        path = Path(member.name)
        if (path.is_absolute() or ".." in path.parts or "\\" in member.name or path in names
                or not (destination / path).resolve().is_relative_to(destination.resolve())
                or not (member.isfile() or member.isdir())):
            raise ValueError(f"Unsafe or duplicate archive member: {member.name}")
        names.add(path)
        total += member.size
        if total > 1024 * 1024 * 1024:
            raise ValueError("Uncompressed archive exceeds 1 GiB")


def check_layout(stage):
    required = [stage / "bin/shop_cli", stage / "RUNNING.txt", stage / SHARE / "licenses/nlohmann-json-LICENSE.MIT"]
    required += [stage / SHARE / guide for guide in GUIDES]
    required += [stage / SHARE / "examples" / name for name in EXAMPLES]
    for path in required:
        if not path.is_file():
            raise ValueError(f"Missing package file: {path}")
    expected = {path.relative_to(stage) for path in required}
    actual = {path.relative_to(stage) for path in stage.rglob("*") if path.is_file()}
    if actual != expected:
        raise ValueError(f"Unexpected package files: {actual - expected}")


def test_installed(stage):
    check_layout(stage)
    subprocess.run([sys.executable, str(LAB / "tools/cli_tests.py"), str(stage / "bin/shop_cli"),
                    "--examples-dir", str(stage / SHARE / "examples")], cwd=stage, check=True, timeout=120)


def verify_archive(archive, docs=False):
    verify_checksum(archive)
    with tempfile.TemporaryDirectory(prefix="ppois-lab3-extracted-") as directory:
        destination = Path(directory)
        with tarfile.open(archive, "r:gz") as stream:
            members = stream.getmembers()
            validate_members(members, destination)
            options = {"filter": "data"} if hasattr(tarfile, "data_filter") else {}
            stream.extractall(destination, members=members, **options)
        stage = destination / package_name(docs)
        if list(destination.iterdir()) != [stage]:
            raise ValueError("Archive must contain one matching native package directory")
        if docs:
            if not (stage / "html/index.html").is_file():
                raise ValueError("Documentation archive has no html/index.html")
        else:
            test_installed(stage)
    print(f"SHA-256 and extracted archive verified: {archive}", flush=True)


def check_release_cache(build):
    cache = {}
    for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if line and not line.startswith(("#", "//")) and "=" in line:
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    if cache.get("CMAKE_BUILD_TYPE") != "Release":
        raise ValueError("Configure a single-configuration Release build before packaging")
    for option in ("BUILD_SHARED_LIBS", "LAB3_ENABLE_COVERAGE", "LAB3_ENABLE_SANITIZERS"):
        value = cache.get(option, "OFF").upper()
        if value not in ("", "0", "OFF", "NO", "FALSE", "N", "IGNORE", "NOTFOUND") and not value.endswith("-NOTFOUND"):
            raise ValueError(f"Release packages must not enable {option}")


def install_application(build, stage):
    check_release_cache(build)
    subprocess.run(["cmake", "--install", str(build), "--prefix", str(stage), "--component", "lab3"], check=True)
    (stage / "RUNNING.txt").write_text(
        "From this directory: ./bin/shop_cli or ./bin/shop_cli --demo\n"
        "Replay: ./bin/shop_cli --script share/ppois_lab3/examples/purchase.input\n"
        "Guide: share/ppois_lab3/docs/CLI.md\n"
        "Use explicit save/load commands for persistent JSON snapshots; no autosave.\n"
        "The initial customers, contacts, inventory and payments are educational examples.\n"
        "No IDE, CMake, Python, Catch2 or JSON installation is needed to run the application.\n"
        f"Native platform: {platform.system()} {platform.machine()}.\n", encoding="utf-8")


def create_package(build, output, docs=False):
    output.mkdir(parents=True, exist_ok=True)
    name = package_name(docs)
    with tempfile.TemporaryDirectory(prefix="ppois-lab3-stage-") as directory:
        workspace = Path(directory)
        stage = workspace / "original" / name
        if docs:
            source = build / "lab3/docs/html"
            if not (source / "index.html").is_file():
                raise ValueError("Build lab3_docs before packaging documentation")
            shutil.copytree(source, stage / "html")
        else:
            install_application(build, stage)
        relocated = workspace / "relocated"
        relocated.mkdir()
        stage.rename(relocated / name)
        if not docs:
            test_installed(relocated / name)
        archive = Path(shutil.make_archive(str(output / name), "gztar", root_dir=relocated, base_dir=name))
        archive.with_name(archive.name + ".sha256").write_text(f"{digest(archive)}  {archive.name}\n", encoding="utf-8")
        verify_archive(archive, docs)
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--docs-only", action="store_true")
    parser.add_argument("--verify-archive", type=Path)
    args = parser.parse_args()
    if args.verify_archive:
        if args.build_dir or args.output_dir:
            parser.error("--verify-archive cannot be combined with build/output directories")
        verify_archive(args.verify_archive.resolve(), args.docs_only)
    else:
        if not args.build_dir or not args.output_dir:
            parser.error("Provide --build-dir and --output-dir, or --verify-archive")
        archive = create_package(args.build_dir.resolve(), args.output_dir.resolve(), args.docs_only)
        print(f"Verified package: {archive}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, tarfile.TarError, subprocess.SubprocessError) as error:
        print(f"Packaging error: {error}", file=sys.stderr)
        sys.exit(1)
