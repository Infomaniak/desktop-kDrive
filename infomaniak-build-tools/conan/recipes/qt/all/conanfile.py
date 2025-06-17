import glob
import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration, ConanException
from conan.tools.files import copy
from conan.tools.system.package_manager import Apt


class QtConan(ConanFile):
    name = "qt"
    version = "6.2.3"
    settings = "os", "arch", "compiler", "build_type"

    options = {
        # ini:       Read the qtaccount.ini file from the user's home directory to get the email and JWT token (default option; if the file does not exist, fallback to envvars)
        # envvars:   Use the environment variable QT_INSTALLER_LOGIN_EMAIL to provide the email to the installer with the --email parameter. The JWT token must be set in the environment variable QT_INSTALLER_JWT_TOKEN.
        # cli:   If the qtaccount.ini file exists, the online installer will use it; otherwise, it will prompt the user for their Qt account email and password. This cannot be used in CI/CD pipelines.
        "qt_login_type": ["ini", "envvars", "cli"],
    }

    default_options = {
        "qt_login_type": "ini",
    }

    def _get_distant_name(self):
        """
        Get the name of the installer to download based on the OS and architecture.
        See 'https://download.qt.io/official_releases/online_installers/' for available installers.
        :return: 'qt-online-installer-{os}-{architecture}-online.{ext}' where os is 'mac', 'linux' or 'windows, architecture is 'arm64' or 'x64' on Linux or Windows and 'x64' on macOS, and ext is 'dmg','run' or 'exe.
        """
        os_map = {"Macos": "mac", "Linux": "linux", "Windows": "windows"}
        ext_map = {"Macos": "dmg", "Linux": "run", "Windows": "exe"}
        os_key = str(self.settings.os)
        os_name = os_map.get(os_key, "windows")
        ext = ext_map.get(os_key, "exe")

        # For macOS, we always use x64 arch because the installer is universal and supports both arm64 and x64.
        if os_name in [ "mac", "windows" ]:
            architecture = "x64"
        else:
            architecture = "arm64" if str(self.settings.arch).startswith("arm") else "x64"

        return f"qt-online-installer-{os_name}-{architecture}-online.{ext}"

    def _get_compiler(self):
        if self.settings.os == "Macos":
            return "clang_64"
        elif self.settings.os == "Linux":
            return "gcc_64"
        elif self.settings.os == "Windows":
            return "win64_msvc2019_64"
        else:
            raise ConanInvalidConfiguration("Unsupported OS for Qt installation")

    def _get_qt_submodules(self, version):
        """
        Get the list of Qt submodules to install based on the version and compiler.
        :param version: The version of Qt to install, e.g. '6.2.3'.
        :return: The list of Qt submodules to install, adapted to the Qt version, the OS, and the compiler
        """
        major = version.split(".")[0]
        compact = version.replace(".", "")
        compiler = self._get_compiler()

        # TODO : Add support for compilers (currently installing all of them, android, ...)
        modules = [
            f"qt.qt{major}.{compact}.{compiler}",
            "qt.tools", "qt.tools.maintenance", # TODO delete this line when the recipe is done

            f"qt.qt{major}.{compact}.qt5compat",
            f"qt.qt{major}.{compact}.src",

            f"qt.qt{major}.{compact}.addons",
                f"qt.qt{major}.{compact}.addons.qtpositioning",
                f"qt.qt{major}.{compact}.addons.qtwebchannel",
                f"qt.qt{major}.{compact}.addons.qtwebengine",
                f"qt.qt{major}.{compact}.addons.qtwebview",
        ]

        if self.settings.os == "Windows":
            modules.append("qt.tools.ninja")
            modules.extend([
                "qt.tools.vcredist",
                "qt.tools.vcredist_64",
                "qt.tools.vcredist_msvc2019_x64",
                "qt.tools.vcredist_msvc2019_x86"
            ])

        return modules

    def _get_default_login_ini_location(self):
        from getpass import getuser
        try:
            user = getuser()
        except OSError:
            return None

        return {
            "Windows": f"C:/Users/{user}/AppData/Roaming/Qt/qtaccount.ini",
            "Macos": f"/Users/{user}/Library/Application Support/Qt/qtaccount.ini",
            "Linux": f"/home/{user}/.local/share/Qt/qtaccount.ini"
        }.get(str(self.settings.os), None)

    def _get_email_from_envvars(self):
        """
        Get the key and email from the environment variables.
        :return: a tuple containing the environment variable key and the email.
        """
        from typing import Final
        qt_email_env_var_key: Final = "QT_INSTALLER_LOGIN_EMAIL"
        email = os.getenv(qt_email_env_var_key)
        if self.options.qt_login_type != "envvars":
            raise ConanInvalidConfiguration(f"Login type '{self.options.qt_login_type}' does not support environment variables. Please use 'envvars' login type.")
        if email is None:
            raise ConanException(f"Environment variable '{qt_email_env_var_key}' is not set. Please set it to your Qt account email.")
        return qt_email_env_var_key, email

    def _check_envvars_login_type(self):
        if self.options.qt_login_type != "envvars":
            return
        key, _ = self._get_email_from_envvars()
        if os.getenv(key) is None:
            raise ConanInvalidConfiguration(f"To be able to use the 'envvars' login type, you must set the environment variable '{key}' with your Qt account email.")
        if os.getenv("QT_INSTALLER_JWT_TOKEN") is None:
            raise ConanInvalidConfiguration("To be able to use the 'envvars' login type, you must set the environment variable 'QT_INSTALLER_JWT_TOKEN' with your Qt account JWT token. See https://doc.qt.io/qt-6/get-and-install-qt-cli.html#providing-login-information")

    def validate(self):
        if self.settings.os not in ["Macos", "Linux", "Windows"]:
            raise ConanInvalidConfiguration("Unsupported OS for Qt installation. Supported OS are: Macos, Linux, Windows.")

        if self.options.qt_login_type == "cli":
            return

        self._check_envvars_login_type()


    def configure(self):
        if self.options.qt_login_type == "ini" and self._get_default_login_ini_location() is None:
            self.output.warning("The file 'qtaccount.ini' was not found in the default location. Falling back to 'envvars' login type")
            self.options.qt_login_type = "envvars"
            self._check_envvars_login_type()


    def _get_executable_path(self, downloaded_file_name: str) -> str:
        """
        On macOS, the downloaded file is a DMG, a disk image. We have to mount it and then find the executable inside.
        Here, we mount the DMG file, and then find the path to the executable inside the mounted DMG, copy the exectable
            to the build folder, unmount the DMG and then return the path to the executable.

        On Linux, the downloaded file is a `.run` file. We `chmod +x` it and can run it directly.

        On Windows, the downloaded file is an `.exe` file, which is an executable. We can run it directly.

        :param downloaded_file_name: The name of the downloaded file.
        :return: the absolute path to the executable.
        """
        if self.settings.os == "Linux":
            exec_path = os.path.abspath(downloaded_file_name)
            os.chmod(exec_path, 0o755)
            return exec_path
        if self.settings.os == "Windows":
            return os.path.abspath(downloaded_file_name) # On Windows, we can run the installer directly
        if self.settings.os != "Macos":
            raise ConanInvalidConfiguration("Unsupported OS for Qt installation")

        # We mount here the DMG file with the options -nobrowse (do not show the mounted volume in Finder), -readonly (mount as read-only), -noautoopen (do not open the mounted volume automatically) and -plist (output in plist format).
        import io
        import plistlib
        output = io.StringIO()
        self.output.highlight("Mounting Qt installer DMG...")
        self.run(
            f"hdiutil attach '{downloaded_file_name}' -nobrowse -readonly -noautoopen -plist",
            stdout=output
        )
        plist = plistlib.loads(output.getvalue().encode("utf-8"))
        mount_point = next(
            (item["mount-point"]
             for item in plist.get("system-entities", [])
             if "mount-point" in item),
            None
        )
        if mount_point is None:
            raise ConanException("Failed to find mount point for the DMG file")

        self.output.highlight("Qt installer DMG mounted at: " + mount_point)
        app_bundles = glob.glob(os.path.join(mount_point, "*.app"))
        if not app_bundles:
            raise ConanException("Failed to find app folder for DMG file")
        mounted_bundle = app_bundles[0]

        app_bundle = os.path.join(self.build_folder, "qt-online-installer-macOS.app")
        from shutil import copytree
        copytree(src=mounted_bundle, dst=app_bundle)
        self._detach_on_macos(mount_point)

        exec_folder = os.path.join(app_bundle, "Contents", "MacOS")
        exec_files = glob.glob(os.path.join(exec_folder, "qt-online-installer-macOS*"))
        if not exec_files:
            raise ConanException("Failed to find executable for Qt installation")

        return exec_files[0]

    def _detach_on_macos(self, mount_point) -> None:
        """
        Detach the mounted DMG file on macOS.
        :param mount_point: The mount point of the DMG file.
        :return: None
        """
        if mount_point is None:
            return
        self.output.highlight("Unounting Qt installer DMG...")
        self.run(f"hdiutil detach '{mount_point}'")

    def build(self):
        self.output.highlight("Downloading Qt installer...")
        downloaded_file_name = self._get_distant_name()
        self.output.highlight("Downloading Qt installer via Python urllib...")
        url = f"https://download.qt.io/official_releases/online_installers/{downloaded_file_name}"
        self.output.info(f"Downloading from: {url}")
        # Si vous avez une erreur de certificat, sur macOS, vous devrez peut-être installer le paquet 'certifi' pour éviter les erreurs SSL via `pip install certifi`
        from urllib.request import urlretrieve
        urlretrieve(url, downloaded_file_name)
        installer_path = self._get_executable_path(downloaded_file_name)

        if not os.path.exists(installer_path):
            raise ConanException("Failed to find installer for Qt installation")
        if not os.access(installer_path, os.X_OK):
            raise ConanException(f"The installer ({installer_path}) is not executable.")

        self.output.highlight("Launching Qt installer...")
        # Run the installer
        # --confirm-command: Confirms starting of installation
        # --accept-obligations: Accepts Qt Open Source usage obligations without user input
        # --accept-licenses: Accepts all licenses without user input.
        # --default-answer: Automatically answers to message queries with their default values.
        process_args =      [ "--confirm-command", "--accept-obligations", "--accept-licenses", "--default-answer" ]
        install_directory = [ "--root", f"'{self.package_folder}'" ]
        process_args = install_directory + process_args

        if self.options.qt_login_type == "envvars":
            _, email = self._get_email_from_envvars()
            process_args = [ "--email", f"'{email}'" ] + process_args

        process_args += [ "install" ] + self._get_qt_submodules(self.version)

        #
        self.run(f"'{installer_path}' {' '.join(process_args)}")

    def package(self):
        self.output.highlight("This step can take a while, please be patient...")
        copy(self, f"{self.version}/", src=self.source_folder, dst=self.package_folder)
        copy(self, "Licenses/", src=self.source_folder, dst=self.package_folder)
        copy(self, "Tools/", src=self.source_folder, dst=self.package_folder)
