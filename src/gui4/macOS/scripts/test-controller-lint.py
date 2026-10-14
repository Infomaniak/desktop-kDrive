"""Regression checks for the macOS 12/13 programmatic-controller lint rules.

Run from src/gui4/macOS with `mise exec -- python3 scripts/test-controller-lint.py`.
"""

import json
import subprocess
import unittest
from pathlib import Path


class ControllerLintTests(unittest.TestCase):
    def lint(self, source):
        result = subprocess.run(
            [
                "swiftlint", "lint", "--config", ".swiftlint.yml",
                "--use-stdin", "--no-cache", "--quiet", "--reporter", "json",
            ],
            input=source,
            text=True,
            capture_output=True,
            cwd=Path(__file__).resolve().parents[1],
            check=False,
        )
        self.assertIn(result.returncode, (0, 2), result.stderr)
        return result.returncode, json.loads(result.stdout)

    def test_unsafe_controllers_fail(self):
        cases = [
            ("final class Unsafe: NSViewController {}", "programmatic_view_controller_inheritance"),
            ("class Unsafe: AppKit.NSViewController {}", "programmatic_view_controller_inheritance"),
            ("class Unsafe<T>: NSViewController {}", "programmatic_view_controller_inheritance"),
            ("class Unsafe:\n    NSViewController {}", "programmatic_view_controller_inheritance"),
            ("let controller = NSViewController()", "bare_nsviewcontroller_init"),
            ("let controller = AppKit.NSViewController()", "bare_nsviewcontroller_init"),
            ("let controller = NSViewController.init(nibName: nil, bundle: nil)", "bare_nsviewcontroller_init"),
        ]
        for source, rule in cases:
            with self.subTest(source=source):
                code, violations = self.lint(source + "\n")
                self.assertEqual(code, 2)
                self.assertEqual([(v["rule_id"], v["severity"]) for v in violations], [(rule, "Error")])

    def test_safe_controllers_and_references_pass(self):
        code, violations = self.lint(
            "final class Safe: BaseViewController {}\n"
            "let controller = BaseViewController()\n"
            "let reference: NSViewController? = nil\n"
            "// class Example: NSViewController {}\n"
            'let example = "NSViewController()"\n'
        )
        self.assertEqual(code, 0)
        self.assertEqual(violations, [])


if __name__ == "__main__":
    unittest.main()
