"""Check package boundaries and unsafe archive handling without publishing anything."""
import importlib.util
from pathlib import Path
import tarfile
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("shop_package", Path(__file__).with_name("package.py"))
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class PackageTests(unittest.TestCase):
    def test_archive_members_reject_traversal_duplicates_and_links(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory)
            for name in ("../evil", "/absolute", "a/../../escape", "a\\escape"):
                with self.subTest(name=name), self.assertRaises(ValueError):
                    PACKAGE.validate_members([tarfile.TarInfo(name)], destination)
            link = tarfile.TarInfo("link")
            link.type = tarfile.SYMTYPE
            with self.assertRaises(ValueError):
                PACKAGE.validate_members([link], destination)
            with self.assertRaises(ValueError):
                PACKAGE.validate_members([tarfile.TarInfo("a"), tarfile.TarInfo("a")], destination)
            with self.assertRaises(ValueError):
                PACKAGE.validate_members([], destination)
            PACKAGE.validate_members([tarfile.TarInfo("regular")], destination)

    def test_checksum_and_release_instrumentation_are_not_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "a.tar.gz"
            archive.write_bytes(b"archive bytes")
            checksum = root / "a.tar.gz.sha256"
            checksum.write_text(f"{PACKAGE.digest(archive)}  a.tar.gz\n", encoding="utf-8")
            PACKAGE.verify_checksum(archive)
            archive.write_bytes(b"changed")
            with self.assertRaises(ValueError):
                PACKAGE.verify_checksum(archive)
            cache = root / "CMakeCache.txt"
            cache.write_text("CMAKE_BUILD_TYPE:STRING=Release\nBUILD_SHARED_LIBS:BOOL=OFF\n", encoding="utf-8")
            PACKAGE.check_release_cache(root)
            for option in ("BUILD_SHARED_LIBS", "LAB3_ENABLE_COVERAGE", "LAB3_ENABLE_SANITIZERS"):
                cache.write_text(f"CMAKE_BUILD_TYPE:STRING=Release\n{option}:BOOL=ON\n", encoding="utf-8")
                with self.assertRaises(ValueError):
                    PACKAGE.check_release_cache(root)


if __name__ == "__main__":
    unittest.main()
