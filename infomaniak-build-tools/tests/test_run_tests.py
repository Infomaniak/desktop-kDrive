"""Regression tests for the shell test runner; no build or live API needed."""

from pathlib import Path
import subprocess
import tempfile
import unittest


RUNNER = Path(__file__).resolve().parents[1] / "run-tests.sh"


class TestRunTests(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory(prefix="kdrive runner ")
        self.addCleanup(self.temporary_directory.cleanup)
        self.root = Path(self.temporary_directory.name)
        self.build = self.root / "build with spaces"
        self.build.mkdir()
        (self.build / "conanrun.sh").write_text("export TEST_RUNTIME_READY=yes\n")

    def run_tests(self, explicit_root=True):
        arguments = ["bash", str(RUNNER)]
        if explicit_root:
            arguments.append(str(self.build))
        return subprocess.run(arguments, cwd=self.build, text=True, capture_output=True, check=False)

    def add_binary(self, name, body):
        binary = self.build / "bin" / name
        binary.parent.mkdir(exist_ok=True)
        binary.write_text("#!/bin/bash\n" + body + "\n")
        return binary

    def test_missing_binaries_fail(self):
        result = self.run_tests()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("No test executables found", result.stderr)

    def test_paths_with_spaces_and_runtime_environment(self):
        self.add_binary("kDrive_test_common", '[ "$TEST_RUNTIME_READY" = yes ] && [ "$PWD" = "$(dirname "$0")" ]')
        result = self.run_tests()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_failure_does_not_skip_other_binaries(self):
        failed = self.add_binary("kDrive_test_failure", 'touch "$0.ran"; exit 7')
        passed = self.add_binary("kDrive_test_success", 'touch "$0.ran"')
        result = self.run_tests()
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(Path(str(failed) + ".ran").exists())
        self.assertTrue(Path(str(passed) + ".ran").exists())

    def test_broken_runtime_environment_fails(self):
        (self.build / "conanrun.sh").write_text("return 1\n")
        binary = self.add_binary("kDrive_test_common", 'touch "$0.ran"')
        self.assertNotEqual(self.run_tests().returncode, 0)
        self.assertFalse(Path(str(binary) + ".ran").exists())

    def test_missing_runtime_environment_fails(self):
        (self.build / "conanrun.sh").unlink()
        self.assertNotEqual(self.run_tests().returncode, 0)

    def test_default_root_still_works(self):
        self.add_binary("kDrive_test_common", "exit 0")
        result = self.run_tests(explicit_root=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_explicit_root_excludes_other_builds(self):
        stale_binary = self.root / "kDrive_test_stale"
        stale_binary.write_text("#!/bin/bash\nexit 1\n")
        self.add_binary("kDrive_test_common", "exit 0")
        result = subprocess.run(
            ["bash", str(RUNNER), str(self.build)], cwd=self.root, text=True, capture_output=True, check=False
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("kDrive_test_stale", result.stdout)


if __name__ == "__main__":
    unittest.main()
