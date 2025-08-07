import glob
import os
from os.path import join as pjoin
import platform

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration, ConanException
from conan.tools.files import copy, rmdir, mkdir


class QtConan(ConanFile):
    name = "qt"
    settings = "os", "arch", "compiler", "build_type"

    options = {
        # ini:       Read the qtaccount.ini file from the user's home directory to get the email and JWT token (default option; if the file does not exist)
        # envvars:   Use the environment variable QT_INSTALLER_JWT_TOKEN to provide the JWT Token to the installer. You can find this token in an existing Qt installation in the file qtaccount.ini inside the folders described in the _get_default_login_ini_location func.
        # cli:   If the qtaccount.ini file exists, the online installer will use it; otherwise, it will prompt the user for their Qt account email and password. This cannot be used in CI/CD pipelines.
        "qt_login_type": ["ini", "envvars", "cli"],
    }

    default_options = {
        "qt_login_type": "ini",
    }

    @staticmethod
    def _get_real_arch():
        return "arm64" if str(platform.machine().lower()) in [ "arm64", "aarch64" ] else "x64"

    @staticmethod
    def _get_distant_name(os_key=None):
        """
        Get the name of the installer to download based on the OS and architecture.
        See 'https://download.qt.io/official_releases/online_installers/' for available installers.
        :return: 'qt-online-installer-{os}-{architecture}-online.{ext}' where os is 'mac', 'linux' or 'windows, architecture is 'arm64' or 'x64' on Linux or Windows and 'x64' on macOS, and ext is 'dmg','run' or 'exe.
        """

        # Here we don't use self.settings.os or self.settings.arch because the settings are not available yet inside the source() method.
        if os_key is None:
            os_key = platform.system().lower()
        data_map = {
            "darwin": [ "mac", "dmg"],
            "linux": [ "linux", "run"],
            "windows": [ "windows", "exe"]
        }
        os_name, ext = data_map.get(os_key, ("windows", "exe"))

        # For macOS, we always use x64 arch because the installer is universal and supports both arm64 and x64.
        if os_name in [ "mac", "windows" ]:
            architecture = "x64"
        else:
            architecture = QtConan._get_real_arch()

        return f"qt-online-installer-{os_name}-{architecture}-online.{ext}"

    def _get_compiler(self):
        if self.settings.os == "Macos":
            return "clang_64"
        elif self.settings.os == "Linux":
            if self._get_real_arch() == "arm64":
                return "linux_gcc_arm64"
            else:
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

            f"qt.qt{major}.{compact}.addons.qtpositioning",
            f"qt.qt{major}.{compact}.addons.qtwebchannel",
            f"qt.qt{major}.{compact}.addons.qtwebengine",
            f"qt.qt{major}.{compact}.addons.qtwebview",
        ]

        if self.settings.build_type == "Debug":
            modules.append(f"qt.qt{major}.{compact}.src") # Add the Qt source files when using debug build type

        if self.settings.os == "Windows":
            modules.append(f"qt.qt{major}.{compact}.debug_info") # Qt Debug Information Files for Windows

        if self.settings.os == "Linux":
            modules.append(f"qt.qt{major}.{compact}.addons.qtserialport")
            modules.append("qt.tools.qtcreator_gui")

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

    def _check_envvars_login_type(self, check_option=True, raise_error=True):
        if check_option and self.options.qt_login_type != "envvars":
            return False
        if os.getenv("QT_INSTALLER_JWT_TOKEN") is None:
            if raise_error:
                raise ConanInvalidConfiguration("To be able to use the 'envvars' login type, you must set the environment variable 'QT_INSTALLER_JWT_TOKEN' with your Qt account JWT token. See https://doc.qt.io/qt-6/get-and-install-qt-cli.html#providing-login-information")
            else:
                return False
        return True

    def config_options(self):
        if self.options.qt_login_type == "ini" and (self._get_default_login_ini_location() is None or not os.path.isfile(self._get_default_login_ini_location())):
            self.output.warning("The file 'qtaccount.ini' is not found at the default location and the login method is 'ini'.")
            if self._check_envvars_login_type(check_option=False, raise_error=False):
                self.output.warning("Falling back to 'envvars' login type.")
                self.options.qt_login_type = "envvars"
            else:
                self.options.qt_login_type = "cli"

    def validate(self):
        if not self.version.startswith("6."):
            raise ConanInvalidConfiguration("This recipe only supports Qt 6.x versions.")

        valid_operating_systems = ["Macos", "Linux", "Windows"]
        if self.settings.os not in valid_operating_systems:
            raise ConanInvalidConfiguration(f"Unsupported OS for Qt installation. Supported OS are: {', '.join(valid_operating_systems)}.")

        if self.options.qt_login_type == "cli":
            return

        self._check_envvars_login_type(check_option=True, raise_error=True)

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
        if len(app_bundles) > 1:
            raise ConanException(f"Found multiple app bundles in the DMG file: {', '.join(app_bundles)}. Please ensure there is only one app bundle in the DMG file.")
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
        self.output.highlight("Unmounting Qt installer DMG...")
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

        self._check_envvars_login_type(check_option=True, raise_error=True) # If the login type is 'envvars', ensure that the required environment variable is set; otherwise, raise an error.

        args += ["install"] + self._get_qt_submodules(self.version)

        quoted_installer = f"'{installer_path}'" if self.settings.os != "Windows" else installer_path

        self.output.highlight(f"Login method: '{self.options.qt_login_type}'")

        self.run(f"{quoted_installer} {' '.join(args)}")

        find_wrap_open_gl = pjoin(self.build_folder, "install", self.version, self._subfolder_install(), "lib", "cmake", "Qt6", "FindWrapOpenGL.cmake")
        if os.path.exists(find_wrap_open_gl) and self.settings.os == "Macos":
            self.output.highlight("Patching Qt installation...")
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
        Get the subfolder name where the Qt installation is done, based on the OS and architecture.
        :return: The subfolder name, e.g., 'macos', 'gcc_64', or 'msvc2019_64'.
        """
        subfolder_map = {
            ("Macos", "arm64"): "macos",
            ("Macos", "x64"): "macos",
            ("Linux", "arm64"): "gcc_arm64",
            ("Linux", "x64"): "gcc_64",
            ("Windows", "arm64"): "msvc2019_64",
            ("Windows", "x64"): "msvc2019_64",
        }
        key = (str(self.settings.os), str(self._get_real_arch()))
        try:
            return subfolder_map[key]
        except KeyError:
            raise ConanInvalidConfiguration(f"Unsupported combination: {key}")

    def package(self):
        self.output.highlight("This step can take a while, please be patient...")

        copy(self, "*",
             src=pjoin(self.build_folder, "install", self.version, self._subfolder_install()),
             dst=self.package_folder
             )

        if self.settings.os == "Linux":
            copy(self, "*SerialPort*",
                 src=pjoin(self.build_folder, "Tools", "QtCreator", "lib", "Qt", "lib"),
                 dst=pjoin(self.package_folder, "lib"),
                 keep_path=False
            )
        elif self.settings.os == "Windows":
            tools_folder = pjoin(self.package_folder, "tools")
            vcredist_folder = pjoin(tools_folder, "vcredist")
            mkdir(self, vcredist_folder)
            copy(self, "*",
                 pjoin(self.build_folder, "install", "Tools"),
                 tools_folder
            )
            copy(self, "vcredist_msvc*",
                 pjoin(self.build_folder, "install", "vcredist"),
                 vcredist_folder,
                 keep_path=False
            )


        for folder in ("doc", "modules"):
            rmdir(self, pjoin(self.package_folder, folder))


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

        for env in (self.runenv_info, self.buildenv_info):
            env.prepend_path("CMAKE_PREFIX_PATH", self.package_folder)
            env.prepend_path("LD_LIBRARY_PATH", pjoin(self.package_folder, "lib"))
            env.prepend_path("DYLD_LIBRARY_PATH", pjoin(self.package_folder, "lib"))
            env.prepend_path("PATH", pjoin(self.package_folder, "bin"))
            if self.settings.os in ("Macos", "Linux"):
                env.prepend_path("PATH", pjoin(self.package_folder, "libexec"))


        self.cpp_info.includedirs = []
        self.cpp_info.bindirs = [ "bin" ]
        if self.settings.os in ("Macos", "Linux"):
            self.cpp_info.bindirs.append("libexec")