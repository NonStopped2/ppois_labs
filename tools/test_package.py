"""Check that lab1 packaging cannot install applications from other laboratories."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location("lab1_package", Path(__file__).with_name("package.py"))
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class PackageScopeTests(unittest.TestCase):
    def test_install_selects_only_lab1_and_propagates_failure(self):
        with patch.object(PACKAGE.subprocess, "run") as run:
            PACKAGE.install_application(Path("build"), Path("stage"))
            run.assert_called_once_with(
                ["cmake", "--install", "build", "--prefix", "stage", "--component", "lab1"], check=True)
            run.side_effect = subprocess.CalledProcessError(1, "cmake")
            with self.assertRaises(subprocess.CalledProcessError):
                PACKAGE.install_application(Path("build"), Path("stage"))

    def test_real_component_install_excludes_other_laboratories(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, build, stage = (root / name for name in ("source", "build", "stage"))
            source.mkdir()
            (source / "cube.txt").write_text("lab1", encoding="utf-8")
            (source / "post.txt").write_text("lab2", encoding="utf-8")
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\nproject(PackageFixture NONE)\n"
                "install(FILES cube.txt DESTINATION share COMPONENT lab1)\n"
                "install(FILES post.txt DESTINATION share COMPONENT lab2)\n", encoding="utf-8")
            subprocess.run(["cmake", "-S", str(source), "-B", str(build)], check=True, capture_output=True)
            PACKAGE.install_application(build, stage)
            self.assertEqual((stage / "share/cube.txt").read_text(encoding="utf-8"), "lab1")
            self.assertFalse((stage / "share/post.txt").exists())


if __name__ == "__main__":
    unittest.main()
