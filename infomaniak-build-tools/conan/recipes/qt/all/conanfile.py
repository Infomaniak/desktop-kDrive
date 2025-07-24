import glob
import os

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

        # We mount here the DMG file with the options -nobrowse (do not show the mounted volume in Finder), -readonly (mount as read-only), -noautoopen (do not open the mounted volume automatically).
        import io
        output = io.StringIO()
        self.output.highlight("Mounting Qt installer DMG...")
        mount_target = os.path.join(self.source_folder, "mnt")
        os.makedirs(mount_target, exist_ok=True)
        self.run(
            f"hdiutil attach '{downloaded_file_name}' -mountpoint '{mount_target}' -nobrowse -readonly -noautoopen",
            stdout=output
        )
        mount_point = mount_target

        self.output.highlight("Qt installer DMG mounted at: " + mount_point)
        app_bundles = glob.glob(os.path.join(mount_point, "*.app"))
        if not app_bundles:
            raise ConanException("Failed to find app folder for DMG file")
        mounted_bundle = app_bundles[0]

        app_bundle = os.path.join(self.build_folder, "qt-online-installer-macOS.app")
        from shutil import copytree
        copytree(src=mounted_bundle, dst=app_bundle)
        self._detach_on_macos(mount_point)
        os.rmdir(mount_point)

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

    def source(self):
        self.output.highlight("Downloading Qt installer via Python urllib...")
        downloaded_file_name = self._get_distant_name()
        url = f"https://download.qt.io/official_releases/online_installers/{downloaded_file_name}"
        self.output.info(f"Downloading from: {url}")
        from urllib.request import urlretrieve
        urlretrieve(url, os.path.join(self.source_folder, downloaded_file_name))

    def build(self):
        installer_path = self._get_executable_path(os.path.join(self.source_folder, self._get_distant_name()))
        if not os.path.exists(installer_path):
            raise ConanException("Failed to find installer for Qt installation")
        if not os.access(installer_path, os.X_OK):
            raise ConanException(f"The installer ({installer_path}) is not executable.")

        if self.settings.os != "Windows":
            installer_path = f"'{installer_path}'" # On Windows, we can't use quotes around the path of the executable.

        self.output.highlight("Launching Qt installer...")
        # Run the installer
        # --confirm-command: Confirms starting of installation
        # --accept-obligations: Accepts Qt Open Source usage obligations without user input
        # --accept-licenses: Accepts all licenses without user input.
        # --default-answer: Automatically answers to message queries with their default values.
        process_args =      [ "--confirm-command", "--accept-obligations", "--accept-licenses", "--default-answer" ]
        install_directory = [ "--root", f"{self.build_folder}/install" ]
        process_args = install_directory + process_args

        if self.options.qt_login_type == "envvars":
            _, email = self._get_email_from_envvars()
            process_args = [ "--email", f"'{email}'" ] + process_args

        process_args += [ "install" ] + self._get_qt_submodules(self.version)

        self.run(f"{installer_path} {' '.join(process_args)}")

    def package(self):
        self.output.highlight("This step can take a while, please be patient...")

        subfolder = "macos" if self.settings.os == "Macos" else ("gcc_64" if self.settings.os == "Linux" else "msvc2019_64")

        copy(self, "*", src=os.path.join(self.build_folder, f"install/{self.version}/{subfolder}/"), dst=self.package_folder)

        rmdir(self, os.path.join(self.package_folder, "doc"))
        rmdir(self, os.path.join(self.package_folder, "modules"))

        # copy(self, "Tools/", src=self.source_folder, dst=self.package_folder)

    def _find_qt_frameworks(self):
        """
        Find the Qt frameworks in the package folder.
        :return: A list of Qt frameworks found in the package folder.
        """
        frameworks_paths = glob.glob(os.path.join(self.package_folder, "lib", "Qt*.framework"))
        if not frameworks_paths:
            raise ConanException("No Qt frameworks found in the package folder.")
        # Get only the name of the frameworks, not the full path
        frameworks_names = [os.path.basename(path) for path in frameworks_paths]
        for framework in frameworks_names:
            self.output.info(f"\t Found Qt framework: {framework}")
        for paths in frameworks_paths:
            self.output.info(f"\t Found Qt framework path: {paths}")
        return frameworks_paths, frameworks_names

    def package_info(self):
        from conan.tools.microsoft import is_msvc
        from conan.tools.apple import is_apple_os
        self.cpp_info.set_property("cmake_file_name", "Qt6")
        self.cpp_info.set_property("pkg_config_name", "qt6")

        self.runenv_info.define("QT_PLUGIN_PATH", os.path.join(self.package_folder, "plugins"))
        self.buildenv_info.define("QT_PLUGIN_PATH", os.path.join(self.package_folder, "plugins"))
        self.buildenv_info.define("QT_HOST_PATH", self.package_folder)


        def _get_corrected_reqs(requires):
            reqs = []
            for r in requires:
                if "::" in r:
                    corrected_req = r
                else:
                    corrected_req = f"qt{r}"
                    assert corrected_req in self.cpp_info.components, f"{corrected_req} required but not yet present in self.cpp_info.components"
                reqs.append(corrected_req)
            return reqs

        def _create_module(module, requires, has_include_dir=True):
            componentname = f"qt{module}"
            assert componentname not in self.cpp_info.components, f"Module {module} already present in self.cpp_info.components"
            self.cpp_info.components[componentname].set_property("cmake_target_name", f"Qt6::{module}")
            self.cpp_info.components[componentname].set_property("pkg_config_name", f"Qt6{module}")
            if module.endswith("Private"):
                libname = module[:-7]
            else:
                libname = module
            self.cpp_info.components[componentname].libs = [f"Qt6{libname}"]
            if has_include_dir:
                self.cpp_info.components[componentname].includedirs = ["include", os.path.join("include", f"Qt{module}")]
            self.cpp_info.components[componentname].defines = [f"QT_{module.upper()}_LIB"]
            if module != "Core" and "Core" not in requires:
                requires.append("Core")
            self.cpp_info.components[componentname].requires = _get_corrected_reqs(requires)

        def _create_plugin(pluginname, libname, plugintype, requires):
            componentname = f"qt{pluginname}"
            assert componentname not in self.cpp_info.components, f"Plugin {pluginname} already present in self.cpp_info.components"
            self.cpp_info.components[componentname].set_property("cmake_target_name", f"Qt6::{pluginname}")
            self.cpp_info.components[componentname].libs = [libname]
            self.cpp_info.components[componentname].libdirs = [os.path.join("plugins", plugintype)]
            self.cpp_info.components[componentname].includedirs = []
            if "Core" not in requires:
                requires.append("Core")
            self.cpp_info.components[componentname].requires = _get_corrected_reqs(requires)

        _create_module("Core", [ "zlib::zlib" ])
        if self.settings.os == "Windows":
            self.cpp_info.components["qtCore"].system_libs.append("authz")
        if is_msvc(self):
            self.cpp_info.components["qtCore"].cxxflags.append("-permissive-")
            self.cpp_info.components["qtCore"].cxxflags.append("-Zc:__cplusplus")
            self.cpp_info.components["qtCore"].system_libs.append("synchronization")
            self.cpp_info.components["qtCore"].system_libs.append("runtimeobject")

        # Frameworks
        fw_paths, fw_names = self._find_qt_frameworks()
        self.cpp_info.frameworks = fw_names
        self.cpp_info.frameworkdirs = fw_paths

        # Libraries
        self.cpp_info.libs = [ "lib", "libexec" ]
        self.cpp_info.bin = [ "bin" ]
        self.cpp_info.include = [ "include" ]


        _create_module("DBus", [])
        gui_reqs = []
        if self.settings.os == "Linux":
            gui_reqs = [ "DBus", "xkbcommon::xkbcommon" ]
        _create_module("Gui", gui_reqs)
        _create_module("Widgets", [])
        _create_module("Network", [])
        _create_module("Sql", [])
        _create_module("Svg", [])
        _create_module("SvgWidgets", [])
        _create_module("WebEngineWidgets", [])
        if self.settings.os == "Windows":
            # https://github.com/qt/qtbase/blob/v6.6.1/src/dbus/CMakeLists.txt#L71-L77
            self.cpp_info.components["qtDBus"].system_libs.append("advapi32")
            self.cpp_info.components["qtDBus"].system_libs.append("netapi32")
            self.cpp_info.components["qtDBus"].system_libs.append("user32")
            self.cpp_info.components["qtDBus"].system_libs.append("ws2_32")

            self.cpp_info.components["qtGui"].system_libs += [
                "advapi32", "gdi32", "ole32", "shell32", "user32", "d3d11", "dxgi", "dxguid"
            ] # https://github.com/qt/qtbase/blob/v6.2.3/src/gui/CMakeLists.txt#L393-L402
            self.cpp_info.components["qtGui"].system_libs.append("d2d1")
            self.cpp_info.components["qtGui"].system_libs.append("dwrite")
        if is_apple_os(self):
            self.cpp_info.components["qtGui"].frameworks = ["CoreFoundation", "CoreGraphics", "CoreText", "Foundation", "ImageIO"]
            if self.settings.os == "Macos":
                _create_plugin("QCocoaIntegrationPlugin", "qcocoa", "platforms", ["Core", "Gui"])
                self.cpp_info.components["QCocoaIntegrationPlugin"].frameworks = [
                    "AppKit", "Carbon", "CoreServices", "CoreVideo", "IOKit", "IOSurface", "Metal", "QuartzCore"
                ]


    def package_id(self):
        self.info.settings.clear()


