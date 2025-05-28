import textwrap

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, cmake_layout
from conan.tools.cmake.toolchain.blocks import VSRuntimeBlock


class KDriveDesktop(ConanFile):

    name = "desktop-kdrive"
    url = "https://github.com/Infomaniak/desktop-kdrive"

    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

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
            # tc.generator = "Ninja"

            # The default VSRuntimeBlock only configures CMAKE_MSVC_RUNTIME_LIBRARY for the profile's build_type
            # (in the CI's profile, it is Release), yielding $<$<CONFIG:Release>:MultiThreadedDLL>. Other configs
            # (RelWithDebInfo, Debug) fall back to MSVC’s default CRT selection (/MT), here, we want /MD
            tc.blocks.remove("vs_runtime")
            tc.blocks["override_vs_runtime"] = OverrideVSRuntimeBlock

        if self.settings.os == "Macos":
            tc.variables["CMAKE_OSX_ARCHITECTURES"] = "x86_64;arm64"
            tc.variables["CMAKE_MACOSX_DEPLOYMENT_TARGET"] = "10.15"
        tc.generate()

    def layout(self):
        cmake_layout(self)

    # def build_requirements(self):
    #     if self.settings.os == "Windows":
    #         self.tool_requires("ninja/1.11.1")

    def requirements(self):
        """
        Specify the dependencies required for this package.
        Here are the dependencies used:
        - `xxhash/0.8.2`: A fast non-cryptographic hash algorithm.
        - ``log4cplus/2.1.2``: A C++ logging library.
        :return: None
        """
        self.requires("xxhash/0.8.2") # From local recipe
        # log4cplus
        log4cplus_options = { "shared": True, "unicode": True }
        if self.settings.os == "Windows":
            log4cplus_options["thread_pool"] = False
        self.requires("log4cplus/2.1.2", options=log4cplus_options) # From https://conan.io/center/recipes/log4cplus

class OverrideVSRuntimeBlock(VSRuntimeBlock):
    template = textwrap.dedent("""\
    cmake_policy(SET CMP0091 NEW)
    message(STATUS "Conan toolchain: Setting CMAKE_MSVC_RUNTIME_LIBRARY=$<$<CONFIG:Debug>:MultiThreadedDebugDLL>$<$<NOT:$<CONFIG:Debug>>:MultiThreadedDLL>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>$<$<NOT:$<CONFIG:Debug>>:MultiThreadedDLL>" CACHE STRING "" FORCE)
    """)
