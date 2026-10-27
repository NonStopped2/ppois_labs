"""Small regression tests for lab2 quality and release tooling, not domain coverage."""
import importlib.util
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch


def load_tool(name):
    spec = importlib.util.spec_from_file_location(f"lab2_{name}", Path(__file__).with_name(f"{name}.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


QUALITY = load_tool("quality")
PACKAGE = load_tool("package")


class InfrastructureTests(unittest.TestCase):
    def test_quality_sources_are_only_lab2_and_include_ui_and_tests(self):
        paths = QUALITY.cpp_sources()
        self.assertTrue(paths)
        self.assertTrue(all(path.is_relative_to(QUALITY.LAB_ROOT) for path in paths))
        for name in ("src/core/Money.cpp", "include/post/core/Money.hpp", "app/main.cpp", "tests/cli/ConsoleTests.cpp"):
            self.assertIn(QUALITY.LAB_ROOT / name, paths)

    def test_formatter_version_is_exactly_pinned(self):
        with patch.object(QUALITY.subprocess, "check_output") as version:
            version.return_value = "clang-format version 18.1.8\n"
            QUALITY.check_formatter("formatter")
            for wrong in ("18.1.80", "19.1.8", "18.1.7", "18.1.8.1", "18.1.8-git"):
                version.return_value = f"clang-format version {wrong}\n"
                with self.assertRaises(ValueError):
                    QUALITY.check_formatter("formatter")

    def test_checksum_rejects_modification_wrong_name_and_malformed_value(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "test.tar.gz"
            archive.write_bytes(b"archive")
            PACKAGE.write_checksum(archive)
            PACKAGE.verify_checksum(archive)
            for value in (f"{'0' * 64}  test.tar.gz\n", f"{PACKAGE.digest(archive)}  other.tar.gz\n", "invalid", "я  test.tar.gz"):
                archive.with_name(archive.name + ".sha256").write_text(value, encoding="utf-8")
                with self.assertRaises(ValueError):
                    PACKAGE.verify_checksum(archive)
            PACKAGE.write_checksum(archive)
            archive.write_bytes(b"tampered")
            with self.assertRaises(ValueError):
                PACKAGE.verify_checksum(archive)

    def test_archive_rejects_paths_links_special_files_duplicates_and_empty_input(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory)
            good = tarfile.TarInfo("package/bin/post_cli")
            PACKAGE.validate_members([good], destination)
            for name in ("../outside", "/absolute", "package/../../outside", "package\\outside"):
                with self.assertRaises(ValueError):
                    PACKAGE.validate_members([tarfile.TarInfo(name)], destination)
            for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE, tarfile.FIFOTYPE):
                member = tarfile.TarInfo("package/bad")
                member.type = kind
                with self.assertRaises(ValueError):
                    PACKAGE.validate_members([member], destination)
            for members in ([], [good, good]):
                with self.assertRaises(ValueError):
                    PACKAGE.validate_members(members, destination)

    def test_release_rejects_debug_shared_and_instrumented_builds(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            cache = build / "CMakeCache.txt"
            cache.write_text("CMAKE_BUILD_TYPE:STRING=Release\n", encoding="utf-8")
            PACKAGE.check_release_cache(build)
            for option in ("BUILD_SHARED_LIBS", "LAB2_ENABLE_COVERAGE", "LAB2_ENABLE_SANITIZERS"):
                for value in ("ON", "TRUE", "1", "YES"):
                    cache.write_text(f"CMAKE_BUILD_TYPE:STRING=Release\n{option}:UNINITIALIZED={value}\n", encoding="utf-8")
                    with self.assertRaises(ValueError):
                        PACKAGE.check_release_cache(build)
            cache.write_text("CMAKE_BUILD_TYPE:STRING=Debug\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                PACKAGE.check_release_cache(build)

    def test_native_package_names_do_not_claim_another_architecture(self):
        with patch.object(PACKAGE.platform, "system", return_value="Darwin"):
            with patch.object(PACKAGE.platform, "machine", return_value="arm64"):
                self.assertEqual(PACKAGE.package_name(), "ppois-lab2-macos-arm64")
        with patch.object(PACKAGE.platform, "system", return_value="Windows"):
            with self.assertRaises(ValueError):
                PACKAGE.package_name()
        self.assertEqual(PACKAGE.package_name(True), "ppois-lab2-docs")


if __name__ == "__main__":
    unittest.main()
