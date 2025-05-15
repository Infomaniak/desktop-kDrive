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
            tc.generator = "Ninja"
        if self.settings.os == "Macos":
            tc.variables["CMAKE_OSX_ARCHITECTURES"] = "x86_64;arm64"
            tc.variables["CMAKE_MACOSX_DEPLOYMENT_TARGET"] = "10.15"
        tc.generate()

    def layout(self):
        cmake_layout(self)

    def build_requirements(self):
        if self.settings.os == "Windows":
            self.tool_requires("ninja/1.11.1")

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
        log4cplus_options = { "shared": False, "unicode": True }
        if self.settings.os == "Windows":
            log4cplus_options["thread_pool"] = False
        self.requires("log4cplus/2.1.2", options=log4cplus_options) # From https://conan.io/center/recipes/log4cplus