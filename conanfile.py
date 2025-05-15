from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, cmake_layout


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
        if self.settings.os == "Macos":
            tc.variables["CMAKE_OSX_ARCHITECTURES"] = "x86_64;arm64"
            tc.variables["CMAKE_MACOSX_DEPLOYMENT_TARGET"] = "10.15"
        tc.generate()

    def layout(self):
        cmake_layout(self)

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
        # Here, by default, log4cplus has an option "LOG4CPLUS_ENABLE_DECORATED_LIBRARY_NAME" that add a U if the unicode option is enabled, ...
        # The conan recipe does not support this and export only this cmake target: `log4cplus::log4cplus`. See https://github.com/conan-io/conan-center-index/blob/99d12797ad75007d4d45734bc67cf541bd250e4f/recipes/log4cplus/all/conanfile.py#L113
        self.requires("log4cplus/2.1.2", options=log4cplus_options) # From https://conan.io/center/recipes/log4cplus