"""Qt recipe authentication regression tests. Requires Conan 2, no Qt account or download."""

import importlib.util
import os
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from conan.errors import ConanInvalidConfiguration

RECIPE_PATH = (Path(__file__).resolve().parents[2] /
               "infomaniak-build-tools/conan/recipes/qt/all/conanfile.py")
spec = importlib.util.spec_from_file_location("kdrive_qt_recipe", RECIPE_PATH)
recipe_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recipe_module)


class QtAuthenticationTests(unittest.TestCase):
    def setUp(self):
        self.environment = patch.dict(os.environ, {}, clear=True)
        self.environment.start()
        self.addCleanup(self.environment.stop)
        self.recipe = recipe_module.QtConan()
        # These tests exercise authentication independent of a compiler profile.
        self.recipe.settings = SimpleNamespace(os="Linux")
        self.recipe.settings_build = SimpleNamespace(get_safe=lambda name: "Linux" if name == "os" else None)
        self.recipe.conf = SimpleNamespace(get=lambda name, default=None, **kwargs: default)
        self.recipe.version = "6.2.3"
        existing_ini = patch.object(recipe_module.os.path, "isfile", return_value=True)
        existing_ini.start()
        self.addCleanup(existing_ini.stop)

    def test_ini_ignores_conflicting_credentials_and_restores_environment_on_failure(self):
        credentials = {"QT_INSTALLER_JWT_TOKEN": "stale-token",
                       "QT_ACCOUNT_EMAIL": "wrong@example.invalid",
                       "QT_ACCOUNT_PASSWORD": "fake-password"}
        os.environ.update(credentials)
        with self.assertRaisesRegex(RuntimeError, "installer failed"):
            with self.recipe._installer_login_environment().apply():
                for key in credentials:
                    self.assertFalse(os.environ.get(key))
                raise RuntimeError("installer failed")
        for key, value in credentials.items():
            self.assertEqual(os.environ[key], value)

    def test_envvars_uses_only_selected_token(self):
        self.recipe.options.qt_login_type = "envvars"
        os.environ.update(QT_INSTALLER_JWT_TOKEN="  chosen-token\n",
                          QT_ACCOUNT_EMAIL="wrong@example.invalid", QT_ACCOUNT_PASSWORD="fake-password")
        with self.recipe._installer_login_environment().apply():
            self.assertEqual(os.environ["QT_INSTALLER_JWT_TOKEN"], "chosen-token")
            self.assertFalse(os.environ.get("QT_ACCOUNT_EMAIL"))
            self.assertFalse(os.environ.get("QT_ACCOUNT_PASSWORD"))
        self.assertEqual(os.environ["QT_INSTALLER_JWT_TOKEN"], "  chosen-token\n")

    def test_build_launches_installer_with_isolated_credentials(self):
        os.environ["QT_INSTALLER_JWT_TOKEN"] = "stale-token"
        self.recipe.folders.set_base_build("/unused/build")

        def run_installer(command):
            self.assertIn(" install ", command)
            self.assertFalse(os.environ.get("QT_INSTALLER_JWT_TOKEN"))
            raise RuntimeError("stop before downloading Qt packages")

        with patch.object(self.recipe, "_download_installer", return_value="installer"), \
                patch.object(self.recipe, "_get_executable_path", return_value="/unused/installer"), \
                patch.object(self.recipe, "_get_qt_submodules", return_value=["qt.qt6.623.clang_64"]), \
                patch.object(recipe_module, "mkdir"), patch.object(self.recipe, "run", side_effect=run_installer):
            with self.assertRaisesRegex(RuntimeError, "stop before downloading"):
                self.recipe.build()
        self.assertEqual(os.environ["QT_INSTALLER_JWT_TOKEN"], "stale-token")

    def test_envvars_rejects_missing_empty_and_whitespace_tokens(self):
        self.recipe.options.qt_login_type = "envvars"
        for token in (None, "", " \n\t"):
            with self.subTest(token=token):
                if token is None:
                    os.environ.pop("QT_INSTALLER_JWT_TOKEN", None)
                else:
                    os.environ["QT_INSTALLER_JWT_TOKEN"] = token
                with self.assertRaisesRegex(ConanInvalidConfiguration, "non-empty"):
                    self.recipe.validate()

    def test_existing_ini_is_preferred_over_environment_token(self):
        os.environ["QT_INSTALLER_JWT_TOKEN"] = "stale-token"
        with patch.object(recipe_module.os.path, "isfile", return_value=True):
            login_type = self.recipe._get_login_type()
        self.assertEqual(login_type, "ini")

    def test_missing_ini_falls_back_to_nonempty_token(self):
        os.environ["QT_INSTALLER_JWT_TOKEN"] = "valid-token"
        with patch.object(recipe_module.os.path, "isfile", return_value=False):
            login_type = self.recipe._get_login_type()
        self.assertEqual(login_type, "envvars")

    def test_missing_ini_in_ci_never_falls_back_to_environment_token(self):
        for marker in ("GITHUB_ACTIONS", "KDRIVE_TEST_CI_RUNNING_ON_CI"):
            for token in ("", "inherited-token"):
                with self.subTest(marker=marker, token=token), patch.dict(
                        os.environ, {marker: "true", "QT_INSTALLER_JWT_TOKEN": token}):
                    with patch.object(recipe_module.os.path, "isfile", return_value=False):
                        with self.assertRaisesRegex(ConanInvalidConfiguration, "Account file not found"):
                            self.recipe.validate()

    def test_ci_rejects_explicit_token_and_interactive_login(self):
        os.environ.update(GITHUB_ACTIONS="true", QT_INSTALLER_JWT_TOKEN="inherited-token")
        for login_type in ("envvars", "cli"):
            with self.subTest(login_type=login_type):
                self.recipe.options.qt_login_type = login_type
                with self.assertRaisesRegex(ConanInvalidConfiguration, "requires qt_login_type=ini"):
                    self.recipe.validate()

    def test_ci_uses_ini_and_ignores_inherited_token(self):
        os.environ.update(GITHUB_ACTIONS="true", QT_INSTALLER_JWT_TOKEN="inherited-token")
        self.recipe.validate()
        with self.recipe._installer_login_environment().apply():
            self.assertFalse(os.environ.get("QT_INSTALLER_JWT_TOKEN"))

    def test_explicit_envvars_does_not_fall_back_to_ini(self):
        self.recipe.options.qt_login_type = "envvars"
        self.assertEqual(self.recipe._get_login_type(), "envvars")
        with self.assertRaisesRegex(ConanInvalidConfiguration, "non-empty"):
            self.recipe.validate()

    def test_interactive_login_remains_available_outside_ci(self):
        with patch.object(recipe_module.os.path, "isfile", return_value=False):
            self.assertEqual(self.recipe._get_login_type(), "cli")
            self.recipe.validate()

    def test_account_paths_follow_runner_environment(self):
        cases = (("Windows", {"APPDATA": "/custom/roaming"}, "/custom/roaming/Qt/qtaccount.ini"),
                 ("Linux", {"XDG_DATA_HOME": "/custom/data"}, "/custom/data/Qt/qtaccount.ini"),
                 ("Macos", {}, "/custom/home/Library/Application Support/Qt/qtaccount.ini"),
                 ("Linux", {}, "/custom/home/.local/share/Qt/qtaccount.ini"))
        for platform, environment, expected in cases:
            with self.subTest(platform=platform, environment=environment), patch.dict(os.environ, environment, clear=True):
                self.recipe.settings.os = platform
                with patch.object(recipe_module.os.path, "expanduser", return_value="/custom/home"):
                    self.assertEqual(Path(self.recipe._get_default_login_ini_location()), Path(expected))


if __name__ == "__main__":
    unittest.main()
