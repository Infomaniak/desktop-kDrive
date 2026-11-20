"""Dependency discovery must support both default and isolated Conan homes."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


HELPERS = Path(__file__).resolve().parents[1] / "conan" / "common-utils.sh"


class TestConanCommonUtils(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory(prefix="kdrive conan ")
        self.addCleanup(self.temporary_directory.cleanup)
        self.root = Path(self.temporary_directory.name)
        self.build = self.root / "build"
        self.build.mkdir()
        self.environment = os.environ.copy()
        self.environment.pop("CONAN_HOME", None)
        self.environment["HOME"] = str(self.root)
        self.environment["LD_LIBRARY_PATH"] = ""

    def discover(self):
        return subprocess.run(
            ["bash", "-c", 'source "$1"; find_qt_conan_path "$2"', "bash", str(HELPERS), str(self.build)],
            env=self.environment,
            text=True,
            capture_output=True,
            check=False,
        )

    def use_generated_environment(self, package):
        (self.build / "conanrun.sh").write_text(f'export LD_LIBRARY_PATH="{package}/lib"\n')

    def test_default_home(self):
        package = self.root / ".conan2/p/b/qt12345/p"
        self.use_generated_environment(package)
        result = self.discover()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), str(package))

    def test_custom_home_with_spaces_and_metacharacters(self):
        home = self.root / "isolated [cache]"
        self.environment["CONAN_HOME"] = str(home)
        for layout in ("p/b/qt12345/p", "p/qt12345/p"):
            with self.subTest(layout=layout):
                package = home / layout
                self.use_generated_environment(package)
                result = self.discover()
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.strip(), str(package))

    def test_active_custom_environment_needs_no_generated_script(self):
        home = self.root / "isolated"
        package = home / "p/b/qt12345/p"
        self.environment["CONAN_HOME"] = str(home)
        self.environment["LD_LIBRARY_PATH"] = str(package / "lib")
        result = self.discover()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), str(package))

    def test_other_cache_is_not_selected(self):
        self.environment["CONAN_HOME"] = str(self.root / "isolated")
        self.use_generated_environment(self.root / ".conan2/p/b/qt12345/p")
        result = self.discover()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Could not find", result.stderr)


if __name__ == "__main__":
    unittest.main()
