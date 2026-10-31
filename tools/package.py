#!/usr/bin/env python3
"""Install and smoke-test native applications, then create a standalone tarball."""
import argparse
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    system = {"Darwin": "macos", "Linux": "linux"}.get(platform.system())
    if not system:
        raise RuntimeError("Release packaging supports native Linux and macOS builds")
    name = f"ppois-lab1-{system}-{platform.machine().lower()}"
    workspace = Path(tempfile.mkdtemp(prefix="stage-", dir=output))
    stage = workspace / name
    subprocess.run(["cmake", "--install", str(build), "--prefix", str(stage)], check=True)
    examples = stage / "share" / "ppois_lab1" / "examples"
    post = subprocess.run([str(stage / "bin" / "post_machine"), str(examples / "post_mark_right.txt")],
                          capture_output=True, text=True, check=True, timeout=20)
    if "HEAD 1" not in post.stdout or "Выполнено команд: 3" not in post.stdout:
        raise RuntimeError("Installed Post machine failed smoke test")
    cube = subprocess.run([str(stage / "bin" / "rubiks_cube"), str(examples / "cube_solved.txt")],
                          input="4\n0\n", capture_output=True, text=True, check=True, timeout=20)
    if "Кубик собран." not in cube.stdout:
        raise RuntimeError("Installed cube failed smoke test")
    (stage / "RUNNING.txt").write_text(
        "Run from this directory:\n"
        "./bin/rubiks_cube\n./bin/post_machine\n"
        "./bin/post_machine share/ppois_lab1/examples/post_increment.txt -log\n"
        "Full instructions: share/ppois_lab1/README.md\n"
        f"Native target: {system} {platform.machine()}. No IDE or CMake is needed to run.\n",
        encoding="utf-8")
    archive = shutil.make_archive(str(output / name), "gztar", root_dir=workspace, base_dir=name)
    print(f"Installed smoke tests passed; archive: {archive}")


if __name__ == "__main__":
    main()
