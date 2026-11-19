#!/usr/bin/env python3
"""Install, relocate, exercise and archive lab4 or verify a downloaded package."""
import argparse
import hashlib
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile

SHARE = Path("share/ppois_lab4")
BINARY = Path("bin/lab4_cli")
DEMO = SHARE / "examples/demo.input"


def package_name(docs):
    if docs:
        return "ppois-lab4-docs"
    system = {"Linux": "linux", "Darwin": "macos"}.get(platform.system())
    if system is None:
        raise ValueError("Lab4 packages support Linux and macOS")
    return f"ppois-lab4-{system}-{platform.machine().lower()}"


def checksum_line(archive):
    digest = hashlib.sha256()
    with archive.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return f"{digest.hexdigest()}  {archive.name}\n"


def extract_package(archive, destination):
    with tarfile.open(archive, "r:gz") as stream:
        files = {}
        for member in stream.getmembers():
            path = Path(member.name)
            if (path.is_absolute() or ".." in path.parts or "\\" in member.name
                    or path in files or not (member.isdir() or member.isfile())):
                raise ValueError(f"Unexpected archive entry: {member.name}")
            files[path] = member
        options = {"filter": "data"} if hasattr(tarfile, "data_filter") else {}
        stream.extractall(destination, members=files.values(), **options)


def check_installed(stage, docs):
    if docs:
        if not (stage / "html/index.html").is_file():
            raise ValueError("Documentation package is missing html/index.html")
        return

    expected_files = {BINARY, SHARE / "README.md", DEMO}
    files = {file.relative_to(stage) for file in stage.rglob("*") if file.is_file()}
    if files != expected_files:
        raise ValueError(f"Unexpected application package contents: {files ^ expected_files}")
    result = subprocess.run([str(stage / BINARY)], cwd=stage, text=True, capture_output=True,
                            input=(stage / DEMO).read_text(encoding="utf-8"), timeout=20, check=True)
    markers = (
        "After: -2 0 1 4 4", "After: -3 1 10 10",
        "After: Fix build (1) Write tests (2) Review docs (2) Ship release (3)",
        "In: 1, out: 2, total: 3", "Arc degree: 2", "vertices: Alpha Beta Gamma",
        "arcs: 2 -> 3 2 -> 1 1 -> 2", "incident: 1 -> 2 2 -> 1 2 -> 3",
        "adjacent: Gamma Alpha",
    )
    if result.stderr or "Error:" in result.stdout or not all(marker in result.stdout for marker in markers):
        raise ValueError(f"Packaged CLI demo failed:\n{result.stdout}\n{result.stderr}")


def verify_archive(archive, docs):
    sidecar = archive.with_name(archive.name + ".sha256")
    if sidecar.read_text(encoding="utf-8") != checksum_line(archive):
        raise ValueError(f"SHA-256 verification failed: {archive.name}")
    with tempfile.TemporaryDirectory(prefix="ppois-lab4-extracted-") as directory:
        destination = Path(directory)
        stage = destination / package_name(docs)
        extract_package(archive, destination)
        if list(destination.iterdir()) != [stage] or not stage.is_dir():
            raise ValueError(f"Expected one package directory named {stage.name}")
        check_installed(stage, docs)
    print(f"Verified SHA-256 and extracted contents: {archive}")


def check_release_cache(build):
    entries = {}
    for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if not line.startswith(("#", "//")) and "=" in line:
            key, value = line.split("=", 1)
            entries[key.split(":", 1)[0]] = value
    if entries.get("CMAKE_BUILD_TYPE") != "Release":
        raise ValueError("Configure a Release build before packaging the CLI")
    for option in ("LAB4_ENABLE_SANITIZERS", "LAB4_ENABLE_COVERAGE"):
        if entries.get(option) != "OFF":
            raise ValueError(f"Configure the release with {option}=OFF")


def create_package(build, output, docs):
    output.mkdir(parents=True, exist_ok=True)
    name = package_name(docs)
    with tempfile.TemporaryDirectory(prefix="ppois-lab4-stage-") as directory:
        workspace = Path(directory)
        stage = workspace / "install" / name
        if docs:
            source = build / "lab4/docs/html"
            shutil.copytree(source, stage / "html")
        else:
            check_release_cache(build)
            subprocess.run(["cmake", "--install", str(build), "--prefix", str(stage),
                            "--component", "lab4"], check=True)
        relocated = workspace / "relocated"
        relocated.mkdir()
        stage.rename(relocated / name)
        check_installed(relocated / name, docs)
        archive = Path(shutil.make_archive(str(output / name), "gztar",
                                           root_dir=relocated, base_dir=name))
        archive.with_name(archive.name + ".sha256").write_text(checksum_line(archive), encoding="utf-8")
        verify_archive(archive, docs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--docs-only", action="store_true")
    parser.add_argument("--verify-archive", type=Path)
    args = parser.parse_args()
    if args.verify_archive:
        if args.build_dir or args.output_dir:
            parser.error("--verify-archive does not use build or output directories")
        verify_archive(args.verify_archive.resolve(), args.docs_only)
    elif args.build_dir and args.output_dir:
        create_package(args.build_dir.resolve(), args.output_dir.resolve(), args.docs_only)
    else:
        parser.error("Provide build and output directories, or --verify-archive")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, tarfile.TarError, subprocess.SubprocessError) as error:
        sys.exit(f"Lab4 package error: {error}")
