"""Regression checks for the macOS 12/13 controller and toolbar lint rules.

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


    def test_parameterless_toolbars_fail(self):
        cases = [
            "NSToolbar()",
            "NSToolbar.init()",
            "AppKit.NSToolbar()",
            "Cocoa.NSToolbar.init()",
            "NSToolbar(\n)",
            "NSToolbar\n    .init(\n)",
        ]
        for expression in cases:
            with self.subTest(expression=expression):
                code, violations = self.lint(f"let toolbar = {expression}\n")
                self.assertEqual(code, 2)
                self.assertEqual(
                    [(v["rule_id"], v["severity"]) for v in violations],
                    [("bare_nstoolbar_init", "Error")],
                )

    def test_explicit_toolbar_identifiers_and_references_pass(self):
        code, violations = self.lint(
            'let toolbar = NSToolbar(identifier: NSToolbar.Identifier("MainToolbar"))\n'
            'let other = AppKit.NSToolbar.init(identifier: NSToolbar.Identifier("OtherToolbar"))\n'
            "let reference: NSToolbar? = nil\n"
            "// NSToolbar() and NSToolbar.init() are unsafe on macOS 12.\n"
            "/* NSToolbar() */\n"
            'let example = "NSToolbar()"\n'
        )
        self.assertEqual(code, 0)
        self.assertEqual(violations, [])


if __name__ == "__main__":
    unittest.main()
