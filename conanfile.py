import os
import textwrap

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMakeToolchain, cmake_layout
from conan.tools.cmake.toolchain.blocks import VSRuntimeBlock


class KDriveDesktop(ConanFile):

    name = "desktop-kdrive"
    url = "https://github.com/Infomaniak/desktop-kdrive"

    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "VirtualRunEnv"

    def validate(self):
        """
        Validate system requirements before building.
        - Only Linux, macOS, and Windows are supported
        - On Ubuntu: minimum version 22.04 for amd64, 24.04 for arm64
        """
        # Check supported operating systems
        supported_os = ["Linux", "Macos", "Windows"]
        if str(self.settings.os) not in supported_os:
            raise ConanInvalidConfiguration(
                f"Unsupported operating system: {self.settings.os}. "
                f"Only {', '.join(supported_os)} are supported."
            )

        # Ubuntu-specific version checks
        if str(self.settings.os) == "Linux":
            self._validate_ubuntu_version()

    def _validate_ubuntu_version(self):
        """
        Validate Ubuntu version based on architecture.
        - amd64 (x86_64): minimum Ubuntu 22.04
        - arm64 (armv8): minimum Ubuntu 24.04
        """
        # Detect if running on Ubuntu
        if not os.path.exists("/etc/os-release"):
            # Not Ubuntu or distro info not available, skip validation
            self.output.warning("Cannot detect Linux distribution. Skipping Ubuntu version check.")
            return

        os_release = self._read_os_release()

        # Check if it's Ubuntu
        distro_id = os_release.get("id", "")
        distro_like = os_release.get("id_like", "")
        if distro_id != "ubuntu" and "ubuntu" not in distro_like.split():
            # Not Ubuntu, skip validation
            return

        # Extract Ubuntu version
        version_id = os_release.get("version_id")
        if not version_id:
            self.output.warning("Cannot detect Ubuntu version. Skipping version check.")
            return

        # Convert version to float for comparison (e.g., "22.04" -> 22.04)
        try:
            ubuntu_version = float(version_id)
        except ValueError:
            self.output.warning(f"Invalid Ubuntu version format: {version_id}. Skipping version check.")
            return

        # Get architecture
        arch = str(self.settings.arch)
        min_versions_by_arch = {
            "x86_64": 22.04,
            "armv8": 24.04,
        }

        min_version = min_versions_by_arch.get(arch)
        if min_version is None:
            self.output.info(f"No Ubuntu version constraint defined for architecture {arch}.")
            return

        if ubuntu_version < min_version:
            raise ConanInvalidConfiguration(
                f"Ubuntu {ubuntu_version} on {arch} is not supported. "
                f"Minimum required version: Ubuntu {min_version} for {arch} architecture."
            )

        self.output.info(f"Ubuntu {ubuntu_version} on {arch} meets minimum requirements.")

    @staticmethod
    def _read_os_release():
        """
        Parse /etc/os-release into a case-normalized dictionary.
        """
        with open("/etc/os-release", "r", encoding="utf-8") as f:
            return {
                key.strip().lower(): value.strip().strip('"').lower()
                for line in f
                if "=" in line and not line.lstrip().startswith("#")
                for key, value in [line.rstrip().split("=", 1)]
            }

    def configure(self):
        if self.settings.os == "Macos":
            self.settings.os.version = "10.15"

    def generate(self):
        """
        Generate the CMake toolchain file.
        Removes the "generic_system" block from the toolchain file to avoid conflicts with msvc on windows.
            - Conan set in the generic_system block the vars "CMAKE_GENERATOR_PLATFORM" and "CMAKE_GENERATOR_TOOLSET" needed by MSBuild but Ninja,
               don't understand it and fails to build.
        """
        tc = CMakeToolchain(self)
        if self.settings.os == "Windows":
            tc.blocks.remove("generic_system")

            # The default VSRuntimeBlock only configures CMAKE_MSVC_RUNTIME_LIBRARY for the profile's build_type
            # (in the CI's profile, it is Release), yielding $<$<CONFIG:Release>:MultiThreadedDLL>. Other configs
            # (RelWithDebInfo, Debug) fall back to MSVC's default CRT selection (/MT), here, we want /MD
            tc.blocks.remove("vs_runtime")
            tc.blocks["override_vs_runtime"] = OverrideVSRuntimeBlock(self, tc, "override_vs_runtime")

        if self.settings.os == "Macos":
            tc.variables["CMAKE_OSX_ARCHITECTURES"] = "x86_64;arm64"
            tc.variables["CMAKE_MACOSX_DEPLOYMENT_TARGET"] = self.settings.os.version

        tc.generate()

        # Post-process: Append normalization code to the generated toolchain
        self._append_conan_vars_normalization()

    def layout(self):
        cmake_layout(self)

    def build_requirements(self):
        if self.settings.os == "Windows":
            self.tool_requires("cmake/[>=3.16.Z]")
            self.tool_requires("ninja/[>=1.11.1]")

    def requirements(self):
        """
        Specify the dependencies required for this package.
        Here are the dependencies used:
        - `xxhash/0.8.2`: A fast non-cryptographic hash algorithm.
        - `log4cplus/2.1.2`: A C++ logging library.
        :return: None
        """
        self.requires("zlib/[>=1.2.11 <2]", transitive_headers=True, options={"shared": True})
        # From local recipe, using the qt online installer.
        # Qt 6.8.3 for Linux ARM, Qt 6.5.3 for other platforms
        qt_version = "6.8.3" if self.settings.os == "Linux" else "6.2.3"
        if self.settings.os == "Linux" and str(self.settings.arch).startswith("arm"):
            self.requires(f"qt/{qt_version}")
        else:
            self.requires(f"qt/{qt_version}")
        self.requires("xxhash/0.8.2") # From local recipe
        # log4cplus
        self.requires("log4cplus/2.1.2", options={
            "shared": True,
            "unicode": True,
            "thread_pool": False if self.settings.os == "Windows" else True
        })

        # openssl depends on zlib, which is already inside the conanfile.py of openssl
        # but since we build openssl two times (for x86_64 and arm64) in single arch and then merge them, we need to add zlib in 'armv8|x86_64' arch mode.
        if self.settings.os == "Macos":
            self.requires("openssl-macos/3.2.4", options={ "shared": True }) # on macOS => Using the local recipe, using the openssl universal build script.
        else:
            self.requires("openssl/3.2.4", options={ "shared": True }) # Otherwise, using the conan center recipe.

        self.requires("sentry/0.7.10", options={ "shared": True, "qt_version": qt_version })
        self.requires("poco/1.13.3", options={ "shared": True })

    def _append_conan_vars_normalization(self):
        """
        Generate a separate CMake helper file for normalizing Conan variables.
        This file must be included AFTER find_package() calls so the variables are defined.
        """
        helper_file = os.path.join(self.generators_folder, "ConanNormalizeVars.cmake")
        normalization_code = textwrap.dedent("""\
            # Normalize Conan variables to be build-type agnostic
            # This file must be included AFTER find_package() calls

            string(TOUPPER ${CMAKE_BUILD_TYPE} _CONAN_BUILD_TYPE_UPPER)

            if(NOT DEFINED _CONAN_PACKAGES)
                message(FATAL_ERROR "No packages defined for Conan variable normalization")
            endif()

            foreach(_pkg ${_CONAN_PACKAGES})
                # Check if the build-type specific variable exists
                if(DEFINED ${_pkg}_LIB_DIRS_${_CONAN_BUILD_TYPE_UPPER})
                    set(${_pkg}_LIB_DIRS "${${_pkg}_LIB_DIRS_${_CONAN_BUILD_TYPE_UPPER}}")
                    message(STATUS "Normalized ${_pkg}_LIB_DIRS from ${_pkg}_LIB_DIRS_${_CONAN_BUILD_TYPE_UPPER} ('${${_pkg}_LIB_DIRS}')")
                # Fallback to RELEASE if current build type not found
                elseif(DEFINED ${_pkg}_LIB_DIRS_RELEASE)
                    set(${_pkg}_LIB_DIRS "${${_pkg}_LIB_DIRS_RELEASE}")
                    message(STATUS "Normalized ${_pkg}_LIB_DIRS from ${_pkg}_LIB_DIRS_RELEASE (fallback)")
                # Fallback to DEBUG if RELEASE not found
                elseif(DEFINED ${_pkg}_LIB_DIRS_DEBUG)
                    set(${_pkg}_LIB_DIRS "${${_pkg}_LIB_DIRS_DEBUG}")
                    message(STATUS "Normalized ${_pkg}_LIB_DIRS from ${_pkg}_LIB_DIRS_DEBUG (fallback)")
                else()
                    message(WARNING "Could not find LIB_DIRS variable for package ${_pkg}")
                endif()
            endforeach()

            unset(_CONAN_BUILD_TYPE_UPPER)
            unset(_CONAN_PACKAGES)
            """)

        with open(helper_file, "w") as f:
            f.write(normalization_code)

class OverrideVSRuntimeBlock(VSRuntimeBlock):
    def __init__(self, conanfile, toolchain, name):
        super().__init__(conanfile, toolchain, name)
        build_type = str(conanfile.settings.build_type)
        if build_type == "Debug":
            runtime = "MultiThreadedDebugDLL"
        else:
            runtime = "MultiThreadedDLL"
        self.template = textwrap.dedent(f"""\
        cmake_policy(SET CMP0091 NEW)
        message(STATUS "Conan toolchain: Setting CMAKE_MSVC_RUNTIME_LIBRARY={runtime}")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "{runtime}" CACHE STRING "" FORCE)
        """)
