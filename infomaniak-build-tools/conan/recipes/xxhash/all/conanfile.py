"""
MIT License

Copyright (c) 2019 Conan.io

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

------------------------------------------------------------------------------------------

Found here: https://github.com/conan-io/conan-center-index/tree/master/recipes/xxhash/
Licensed under the MIT License.

"""
import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, export_conandata_patches, get, rmdir

required_conan_version = ">=1.53.0"


class XxHashConan(ConanFile):
    name = "xxhash"
    description = "Extremely fast non-cryptographic hash algorithm"
    license = "BSD-2-Clause"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://github.com/Cyan4973/xxHash"
    topics = ("hash", "algorithm", "fast", "checksum", "hash-functions")
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "utility": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": False,
        "utility": True,
    }

    def export_sources(self):
        export_conandata_patches(self)

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        if self.settings.os == "Macos":
            self.settings.os.version = "11.0"

    def layout(self):
        cmake_layout(self, src_folder="src")

    def source(self):
        get(self, f"https://github.com/Cyan4973/xxHash/archive/v{self.version}.tar.gz", strip_root=True)

    def build_requirements(self):
        if self.settings.os == "Windows":
            self.tool_requires("ninja/[>=1.11.1]")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["CMAKE_MACOSX_BUNDLE"] = False
        tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5" # CMake 4 support
        tc.cache_variables["XXHASH_BUILD_XXHSUM"] = False
        tc.cache_variables["CMAKE_MACOSX_RPATH"] = True
        # Use Ninja in windows.
        if self.settings.os == "Windows":
            tc.cache_variables["CMAKE_GENERATOR"] = "Ninja"
            tc.generator = "Ninja"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, "cmake_unofficial"), variables={ "CMAKE_BUILD_TYPE": self.forced_build_type } if self.settings.os != "Windows" else None)
        cmake.build()

    @property
    def forced_build_type(self):
        return "Release" # Force the build type to Release since we don't need to debug symbols

    def package_id(self):
        self.info.settings.rm_safe("build_type")

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "xxHash")
        self.cpp_info.set_property("cmake_target_name", "xxHash::xxhash")
        self.cpp_info.set_property("pkg_config_name", "libxxhash")
        self.cpp_info.components["libxxhash"].libs = ["xxhash"]
        self.cpp_info.components["libxxhash"].set_property("cmake_target_name", "xxHash::xxhash")
        self.cpp_info.components["libxxhash"].includedirs.append(os.path.join(self.package_folder, "include"))
