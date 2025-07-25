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
        valid_operating_systems = ["Macos", "Linux", "Windows"]
        if self.settings.os not in valid_operating_systems:
            raise ConanInvalidConfiguration(f"Unsupported OS for Qt installation. Supported OS are: {', '.join(valid_operating_systems)}.")

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
        self._check_integrity()


    def package_info(self):
        from conan.tools.microsoft import is_msvc
        from conan.tools.apple import is_apple_os

        # --- Propriétés globales ---
        self.cpp_info.set_property("cmake_file_name", "Qt6")
        self.cpp_info.set_property("pkg_config_name", "qt6")

        # --- Variables d’environnement pour les plugins Qt ---
        qt_plugins = os.path.join(self.package_folder, "plugins")
        self.runenv_info.define("QT_PLUGIN_PATH", qt_plugins)
        self.buildenv_info.define("QT_PLUGIN_PATH", qt_plugins)
        self.buildenv_info.define("QT_HOST_PATH", self.package_folder)

        # --- Helpers internes ---
        def _fix_requires(reqs):
            corrected = []
            for r in reqs:
                if "::" in r:
                    corrected.append(r)
                else:
                    comp = f"qt{r}"
                    assert comp in self.cpp_info.components, f"Component '{comp}' not found in Qt components."
                    corrected.append(comp)
            return corrected

        def _add_module(name, requires=None, has_include=True):
            requires = requires or []
            comp_name = f"qt{name}"
            comp = self.cpp_info.components[comp_name]
            # CMake & pkg-config
            comp.set_property("cmake_target_name", f"Qt6::{name}")
            comp.set_property("pkg_config_name", f"qt6{name.lower()}")
            # Bibliothèque
            comp.frameworks = [ f"Qt{name}" ]
            comp.frameworkdirs = [ "lib" ]
            # Includes
            if has_include:
                comp.includedirs = [
                    os.path.join("include"),
                    os.path.join("lib", f"Qt{name}.framework", "Headers")
                ]
            # Macro & flags
            comp.defines = [f"QT_{name.upper()}_LIB"]
            # Dépendances internes (Core toujours présent sauf pour Core)
            deps = (["Core"] if name != "Core" else []) + requires
            comp.requires = _fix_requires(deps)

        def _add_plugin(name, libname, folder, requires=None):
            requires = requires or []
            comp_name = f"qt{name}"
            comp = self.cpp_info.components[comp_name]
            comp.set_property("cmake_target_name", f"Qt6::{name}")
            comp.set_property("pkg_config_name", f"qt6{name.lower()}")
            comp.libs = [libname]
            comp.libdirs = [os.path.join("plugins", folder)]
            comp.requires = _fix_requires(["Core"] + requires)

        _add_module("Core", requires=["zlib::zlib"])
        if is_msvc(self):
            core = self.cpp_info.components["qtCore"]
            core.cxxflags.extend(["-permissive-", "-Zc:__cplusplus"])
            core.system_libs.extend(["synchronization", "runtimeobject"])

        modules = {
            # Here, the order of the modules is important, the dependencies must be declared before the modules that depend on them.
            "DBus": [],
            "Gui": ["DBus"] if self.settings.os == "Linux" else [],
            "Widgets": ["Gui"],
            "Network": [],
            "Sql": [],
            "Svg": ["Widgets", "Gui"],
            "SvgWidgets": ["Svg", "Widgets", "Gui"],
            "Qml": ["Network"],
            "QmlModels": ["Qml", "Network"],
            "OpenGL": ["Gui"],
            "Quick": ["QmlModels", "OpenGL", "Gui"],
            "WebChannel": ["Qml", "Network"],
            "Positioning": [],
            "WebEngineCore": ["Quick", "QmlModels", "OpenGL", "Gui", "WebChannel", "Qml", "Network", "Positioning"],
            "WebEngineWidgets": ["WebEngineCore"],
        }
        for mod, reqs in modules.items():
            _add_module(mod, requires=reqs)

        if self.settings.os == "Windows":
            self.cpp_info.components["qtDBus"].system_libs += ["advapi32", "netapi32", "user32", "ws2_32"]
            self.cpp_info.components["qtGui"].system_libs += [
                "advapi32", "gdi32", "ole32", "shell32", "user32",
                "d3d11", "dxgi", "dxguid", "d2d1", "dwrite"
            ]

        if is_apple_os(self):
            gui = self.cpp_info.components["qtGui"]
            gui.frameworks = ["CoreFoundation", "CoreGraphics", "CoreText", "Foundation", "ImageIO"]
            gui.frameworkdirs = []  # utilise les frameworks système

            _add_plugin("QCocoaIntegrationPlugin", "qcocoa", "platforms", requires=["Gui"])
            cocoa = self.cpp_info.components["qtQCocoaIntegrationPlugin"]
            cocoa.frameworks = [
                "AppKit", "Carbon", "CoreServices", "CoreVideo",
                "IOKit", "IOSurface", "Metal", "QuartzCore"
            ]
            cocoa.frameworkdirs = [os.path.join(self.package_folder, "lib")]

        self.cpp_info.bindirs = ["bin", "libexec"]
        self.cpp_info.libdirs = ["lib"]
        self.cpp_info.includedirs = ["include"]

        cmake_folder = os.path.join(self.package_folder, "cmake")
        find_modules = []
        concerned_modules = list(modules.keys())
        concerned_modules.append("Core")
        if os.path.isdir(cmake_folder):
            for fname in os.listdir(cmake_folder):
                for module_name in modules:
                    if fname.lower().endswith(".cmake"):
                        path = os.path.join("lib", "cmake", f"Qt6{module_name}", fname)
                        find_modules.append(path)
                        self.output.highlight("Found CMake module: " + path)
        if find_modules:
            self.cpp_info.set_property("cmake_build_modules", find_modules)

        self._setup_environment()




    def package_id(self):
        self.info.settings.clear()
        
    def _check_integrity(self):
        expected_paths = [
            os.path.join(self.package_folder, "bin"),
            os.path.join(self.package_folder, "lib"),
            os.path.join(self.package_folder, "libexec"),
            os.path.join(self.package_folder, "include"),
        ]

        for path in expected_paths:
            if not os.path.exists(path):
                raise ConanException(f"Missing expected file or directory: {path}")

    def _setup_environment(self):
        bin_path = os.path.join(self.package_folder, "bin")
        libexec_path = os.path.join(self.package_folder, "libexec")
        qt_plugins = os.path.join(self.package_folder, "plugins")
        platforms_path = os.path.join(qt_plugins, "platforms")

        # PATH
        self.buildenv_info.prepend_path("PATH", bin_path)
        self.buildenv_info.prepend_path("PATH", libexec_path)
        self.runenv_info.prepend_path("PATH", bin_path)
        self.runenv_info.prepend_path("PATH", libexec_path)

        # Qt plugin paths
        self.buildenv_info.define("QT_PLUGIN_PATH", qt_plugins)
        self.runenv_info.define("QT_PLUGIN_PATH", qt_plugins)
        self.buildenv_info.define("QT_QPA_PLATFORM_PLUGIN_PATH", platforms_path)
        self.runenv_info.define("QT_QPA_PLATFORM_PLUGIN_PATH", platforms_path)

        # Optionnel pour tooling ou override
        self.buildenv_info.define_path("QT_TOOLS_PATH", bin_path)
        self.buildenv_info.define_path("QT_LIBEXEC_PATH", libexec_path)
        self.buildenv_info.define_path("QT_LIB_DIR", os.path.join(self.package_folder, "lib"))
        self.buildenv_info.define_path("QT_INCLUDE_DIR", os.path.join(self.package_folder, "include"))
        self.buildenv_info.define_path("QT_HOST_PATH", self.package_folder)

        # Pour aider CMake à trouver Qt
        self.buildenv_info.prepend_path("CMAKE_PREFIX_PATH", self.package_folder)

        # Pour le debug des plugins Qt
        self.runenv_info.define("QT_DEBUG_PLUGINS", "1")