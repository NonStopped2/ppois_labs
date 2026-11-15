#!/usr/bin/env python3
"""Install, relocate and test lab2 archives; verify downloaded packages separately."""
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

LAB_ROOT = Path(__file__).resolve().parents[1]
SHARE = Path("share/ppois_lab2")
EXAMPLES = ("transport.input", "courier.input", "loss.input")
GUIDES = ("README.md", "CLI.md", "BEHAVIORS.md", "CI.md")


def package_name(docs=False):
    if docs:
        return "ppois-lab2-docs"
    system = {"Darwin": "macos", "Linux": "linux"}.get(platform.system())
    if system is None:
        raise ValueError("Native release packaging supports Linux and macOS")
    return f"ppois-lab2-{system}-{platform.machine().lower()}"


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def write_checksum(archive):
    archive.with_name(archive.name + ".sha256").write_text(f"{digest(archive)}  {archive.name}\n", encoding="utf-8")


def verify_checksum(archive):
    fields = archive.with_name(archive.name + ".sha256").read_text(encoding="utf-8").split()
    if (len(fields) != 2 or fields[1] != archive.name or not re.fullmatch(r"[0-9a-f]{64}", fields[0])
            or not hmac.compare_digest(fields[0], digest(archive))):
        raise ValueError(f"Checksum mismatch: {archive}")


def validate_members(members, destination):
    if not members:
        raise ValueError("Empty archive")
    names = set()
    for member in members:
        path = Path(member.name)
        if (path.is_absolute() or ".." in path.parts or "\\" in member.name
                or not (destination / path).resolve().is_relative_to(destination.resolve())
                or not (member.isfile() or member.isdir()) or path in names):
            raise ValueError(f"Unsafe or duplicate archive member: {member.name}")
        names.add(path)


def check_layout(stage):
    required = [stage / "bin/post_cli", stage / "RUNNING.txt"]
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
    # Используем именно установленные примеры и запускаем процесс вне исходного дерева.
    subprocess.run([sys.executable, str(LAB_ROOT / "tools/cli_tests.py"), str(stage / "bin/post_cli"),
                    "--examples-dir", str(stage / SHARE / "examples")], check=True, cwd=stage, timeout=120)


def verify_archive(archive, docs=False):
    verify_checksum(archive)
    with tempfile.TemporaryDirectory(prefix="ppois-lab2-extracted-") as directory:
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
    print(f"Extracted archive and SHA-256 verified: {archive}", flush=True)


def check_release_cache(build):
    cache = {}
    for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if line and not line.startswith(("#", "//")) and "=" in line:
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    if cache.get("CMAKE_BUILD_TYPE") != "Release":
        raise ValueError("Configure a single-configuration Release build before packaging")
    for option in ("BUILD_SHARED_LIBS", "LAB2_ENABLE_COVERAGE", "LAB2_ENABLE_SANITIZERS"):
        value = cache.get(option, "OFF").upper()
        if value not in ("", "0", "OFF", "NO", "FALSE", "N", "IGNORE", "NOTFOUND") and not value.endswith("-NOTFOUND"):
            raise ValueError(f"Release packages must not enable {option}")


def install_application(build, stage):
    check_release_cache(build)
    subprocess.run(["cmake", "--install", str(build), "--prefix", str(stage), "--component", "lab2"], check=True)
    (stage / "RUNNING.txt").write_text(
        "From this directory, run ./bin/post_cli or ./bin/post_cli --demo\n"
        "Replay: ./bin/post_cli --demo < share/ppois_lab2/examples/transport.input\n"
        "Guide: share/ppois_lab2/CLI.md\n"
        f"Native platform: {platform.system()} {platform.machine()}.\n"
        "No IDE, CMake, Python or Catch2 is needed to run the application.\n"
        "State is in memory only and is lost when the process exits.\n", encoding="utf-8")


def create_package(build, output, docs=False):
    output.mkdir(parents=True, exist_ok=True)
    name = package_name(docs)
    with tempfile.TemporaryDirectory(prefix="ppois-lab2-stage-") as directory:
        workspace = Path(directory)
        stage = workspace / "original" / name
        if docs:
            source = build / "lab2/docs/html"
            if not (source / "index.html").is_file():
                raise ValueError("Build the lab2_docs target before packaging documentation")
            shutil.copytree(source, stage / "html")
        else:
            install_application(build, stage)
        # Перенос обнаруживает зависимости от первоначального install prefix.
        relocated = workspace / "relocated"
        relocated.mkdir()
        stage.rename(relocated / name)
        if not docs:
            test_installed(relocated / name)
        archive = Path(shutil.make_archive(str(output / name), "gztar", root_dir=relocated, base_dir=name))
        write_checksum(archive)
        verify_archive(archive, docs)
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--docs-only", action="store_true")
    parser.add_argument("--verify-archive", type=Path,
                        help="Verify adjacent .sha256 and run an extracted native archive without rebuilding")
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
