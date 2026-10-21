import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, rmdir
from conan.tools.system import package_manager

required_conan_version = ">=1.53.0"


class KeychainConan(ConanFile):
    name = "keychain"
    description = "A pragmatic C++ library to store passwords securely on multiple platforms"
    license = "MIT"
    url = "https://github.com/hrantzsch/keychain"
    homepage = "https://github.com/hrantzsch/keychain"
    topics = ("password", "keychain", "credentials", "security")
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def source(self):
        get(self, f"https://github.com/hrantzsch/keychain/archive/refs/tags/v{self.version}.tar.gz", strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_TESTS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        # Keep the same CMake target name as the upstream library to preserve existing consumers.
        self.cpp_info.set_property("cmake_file_name", "keychain")
        self.cpp_info.set_property("cmake_target_name", "keychain")
        # Without an explicit lib name, CMakeDeps generates a target that never links
        # libkeychain and consumers fail at link time with undefined symbols.
        self.cpp_info.libs = ["keychain"]
        # The system libraries are linked privately by the upstream CMakeLists; since the package is
        # built as a static library, they must be propagated to the consumers.
        if self.settings.os == "Macos":
            self.cpp_info.frameworks = ["CoreFoundation", "Security"]
        elif self.settings.os == "Windows":
            self.cpp_info.system_libs = ["crypt32"]
        else:
            self.cpp_info.system_libs = ["glib-2.0", "secret-1"]
