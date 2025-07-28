import glob
import os
from os.path import join as pjoin

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration, ConanException
from conan.tools.files import copy, rmdir


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

    @staticmethod
    def _get_distant_name(os_key=None):
        """
        Get the name of the installer to download based on the OS and architecture.
        See 'https://download.qt.io/official_releases/online_installers/' for available installers.
        :return: 'qt-online-installer-{os}-{architecture}-online.{ext}' where os is 'mac', 'linux' or 'windows, architecture is 'arm64' or 'x64' on Linux or Windows and 'x64' on macOS, and ext is 'dmg','run' or 'exe.
        """
        import platform # Here we don't use self.settings.os or self.settings.arch because the settings are not available yet inside the source() method.
        if os_key is None:
            os_key = platform.system().lower()
        data_map = {
            "darwin": [ "mac", "dmg"],
            "linux": [ "linux", "run"],
            "windows": [ "windows", "exe"]
        }
        os_name = data_map.get(os_key, "windows")[0]
        ext = data_map.get(os_key, "exe")[1]

        # For macOS, we always use x64 arch because the installer is universal and supports both arm64 and x64.
        if os_name in [ "mac", "windows" ]:
            architecture = "x64"
        else:
            architecture = "arm64" if str(platform.machine().lower()) in [ "arm64", "aarch64" ] else "x64"

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

        modules = [
            f"qt.qt{major}.{compact}.{compiler}",

            f"qt.qt{major}.{compact}.qt5compat",
            f"qt.qt{major}.{compact}.src",

            f"qt.qt{major}.{compact}.addons.qtpositioning",
            f"qt.qt{major}.{compact}.addons.qtwebchannel",
            f"qt.qt{major}.{compact}.addons.qtwebengine",
            f"qt.qt{major}.{compact}.addons.qtwebview",
        ]

        if self.settings.os == "Windows":
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

        if user == "root":
            return {
                "Windows": "C:/Users/root/AppData/Roaming/Qt/qtaccount.ini",
                "Macos": "/var/root/Library/Application Support/Qt/qtaccount.ini",
                "Linux": "/root/.local/share/Qt/qtaccount.ini"
            }.get(str(self.settings.os), None)
        else:
            return {
                "Windows": f"C:/Users/{user}/AppData/Roaming/Qt/qtaccount.ini",
                "Macos": f"/Users/{user}/Library/Application Support/Qt/qtaccount.ini",
                "Linux": f"/home/{user}/.local/share/Qt/qtaccount.ini"
            }.get(str(self.settings.os), None)

    def requirements(self):
        self.requires("zlib/[>=1.2.11 <2]", options={ "shared": True }) # From https://conan.io/center/recipes/zlib

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
        if not self.version.startswith("6."):
            raise ConanInvalidConfiguration("This recipe only supports Qt 6.x versions.")

        valid_operating_systems = ["Macos", "Linux", "Windows"]
        if self.settings.os not in valid_operating_systems:
            raise ConanInvalidConfiguration(f"Unsupported OS for Qt installation. Supported OS are: {', '.join(valid_operating_systems)}.")

        if self.options.qt_login_type == "cli":
            return

        self._check_envvars_login_type()


    def configure(self):
        if self.options.qt_login_type == "ini" and (self._get_default_login_ini_location() is None or not os.path.isfile(self._get_default_login_ini_location())):
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

        # We mount here the DMG file with the options -nobrowse (do not show the mounted volume in Finder), -readonly (mount as read-only), -noautoopen (do not open the mounted volume automatically).
        import io
        output = io.StringIO()
        self.output.highlight("Mounting Qt installer DMG...")
        mount_point = pjoin(self.source_folder, "mnt")
        os.makedirs(mount_point, exist_ok=False)
        self.run(
            f"hdiutil attach '{downloaded_file_name}' -mountpoint '{mount_point}' -nobrowse -readonly -noautoopen",
            stdout=output
        )

        self.output.highlight("Qt installer DMG mounted at: " + mount_point)
        app_bundles = glob.glob(pjoin(mount_point, "*.app"))
        if not app_bundles:
            raise ConanException("Failed to find app folder for DMG file")
        mounted_bundle = app_bundles[0]

        app_bundle = pjoin(self.build_folder, "qt-online-installer-macOS.app")
        from shutil import copytree
        # Copy the bundle from the mounted DMG to the build folder so we can umount the DMG.
        copytree(src=mounted_bundle, dst=app_bundle)
        self._detach_on_macos(mount_point)
        os.rmdir(mount_point)

        exec_folder = pjoin(app_bundle, "Contents", "MacOS")
        exec_files = glob.glob(pjoin(exec_folder, "qt-online-installer-macOS*")) # Find the executable file in the MacOS folder of the app bundle.
        if not exec_files:
            raise ConanException("Failed to find executable for Qt installation")

        return exec_files[0]

    def _detach_on_macos(self, mount_point) -> None:
        """
        Detach the mounted DMG file on macOS.
        :param mount_point: The mount point of the DMG file.
        :return: None
        """
        if mount_point is None or self.settings.os != "Macos":
            return
        self.output.highlight("Unounting Qt installer DMG...")
        self.run(f"hdiutil detach '{mount_point}'")

    def source(self):
        self.output.highlight("Downloading Qt installer...")
        downloaded_file_name = self._get_distant_name()
        url = f"https://download.qt.io/official_releases/online_installers/{downloaded_file_name}"
        self.output.info(f"Downloading from: {url}")
        from urllib.request import urlretrieve
        urlretrieve(url, pjoin(self.source_folder, downloaded_file_name))

    def build(self):
        self.output.highlight("Launching Qt installer...")
        installer_path = self._get_executable_path(pjoin(self.source_folder, self._get_distant_name()))

        args = [
            "--confirm-command",     # Confirms starting of installation
            "--accept-obligations",  # Accepts Qt Open Source usage obligations without user input
            "--accept-licenses",     # Accepts all licenses without user input.
            "--default-answer"       # Automatically answers to message queries with their default values.
        ]

        args = ["--root", f"{self.build_folder}/install"] + args

        if self.options.qt_login_type == "envvars":
            args = ["--email", self._get_email_from_envvars()[1]] + args

        args += ["install"] + self._get_qt_submodules(self.version)

        quoted_installer = f"'{installer_path}'" if self.settings.os != "Windows" else installer_path
        self.run(f"{quoted_installer} {' '.join(args)}")

        self.output.highlight("Patching Qt installation...")
        find_wrap_open_gl = pjoin(self.build_folder, "install", self.version, self._subfolder_install(), "lib", "cmake", "Qt6", "FindWrapOpenGL.cmake")
        if os.path.exists(find_wrap_open_gl) and self.settings.os == "Macos":
            from conan.tools.files import replace_in_file
            """
            Fixes spam of this kind of CMake warning on macOS:            
            
            CMake Warning at /Users/username/.conan2/p/b/qte693ca8061042/p/lib/cmake/Qt6/FindWrapOpenGL.cmake:41 (target_link_libraries):
              Target "kDrive_client" requests linking to directory
              "/Applications/Xcode-16.1.0.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX15.1.sdk/System/Library/Frameworks".
              Targets may link only to libraries.  CMake is dropping the item.
            Call Stack (most recent call first):
              /Applications/CMake.app/Contents/share/cmake-3.31/Modules/CMakeFindDependencyMacro.cmake:76 (find_package)
              src/CMakeLists.txt:1 (find_package)
            """
            replace_in_file(
                self, find_wrap_open_gl,
                "target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE ${__opengl_fw_path})",
                "target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE ${__opengl_fw_path}/OpenGL.framework)"
            )

    @staticmethod
    def _validate_installer_path(path):
        if not os.path.exists(path):
            raise ConanException(f"Installer not found: {path}")
        if not os.access(path, os.X_OK):
            raise ConanException(f"The installer is not executable: {path}")


    def _subfolder_install(self):
        """
        Get the subfolder name where the Qt installation is done, based on the OS.
        :return: The subfolder name where the Qt installation is done, e.g. 'macos', 'gcc_64' or 'msvc2019_64'.
        """
        subfolder_map = {
            "Macos": "macos",
            "Linux": "gcc_64",
            "Windows": "msvc2019_64"
        }
        return subfolder_map.get(str(self.settings.os))

    def package(self):
        self.output.highlight("This step can take a while, please be patient...")

        src_path = pjoin(self.build_folder, "install", self.version, self._subfolder_install()) # "./install/6.2.3/gcc_64" or "./install/6.2.3/msvc2019_64" or "./install/6.2.3/macos"
        dst_path = self.package_folder

        copy(self, "*", src=src_path, dst=dst_path)

        for folder in ("doc", "modules"):
            rmdir(self, pjoin(dst_path, folder))


    def package_info(self):
        """
        Sets the package information for Qt, including CMake configuration and environment variables.
        Here, we do not set up detailed package information. Since Qt is installed using the online installer,
        it provides the necessary CMake configuration files to locate the package, its submodules, and components.
        We only specify the main CMake configuration file so that CMake can find the package.
        """
        self.cpp_info.set_property("cmake_file_name", "Qt6")
        self.cpp_info.set_property("cmake_build_modules", [ pjoin(self.package_folder, "lib", "cmake", "Qt6", "Qt6Config.cmake") ])
        self.cpp_info.set_property("cmake_find_mode", "none")

        self.buildenv_info.prepend_path("CMAKE_PREFIX_PATH", self.package_folder)
        self.runenv_info.prepend_path("CMAKE_PREFIX_PATH", self.package_folder)

        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.bindirs = []
        self.cpp_info.resdirs = []
        self.cpp_info.srcdirs = []
        self.cpp_info.frameworkdirs = []