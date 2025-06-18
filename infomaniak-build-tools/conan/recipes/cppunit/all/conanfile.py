import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.apple import is_apple_os, fix_apple_shared_install_name
from conan.tools.files import copy


class CPPUnitUniversalConan(ConanFile):
    name = "cppunit-universal"
    version = "1.15.1"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
    }
    default_options = {
        "shared": True,
    }
    exports_sources = "cppunit_universal_build.sh"


    def validate(self):
        if not is_apple_os(self):
            raise ConanInvalidConfiguration("cppunit-universal is only supported on Apple platforms (macOS, iOS, etc.)")

    def build(self):
        script = os.path.join(self.build_folder, "cppunit_universal_build.sh")

        self.run(f"chmod +x {script}")
        self.run(f"bash {script} --build-folder {self.build_folder}")

    def package(self):
        copy(self, "*.h", src=os.path.join(self.build_folder, "cppunit.multi", "include"), dst=os.path.join(self.package_folder, "include"))
        copy(self, "*.dylib", src=os.path.join(self.build_folder, "cppunit.multi", "lib"), dst=os.path.join(self.package_folder, "lib"))
        copy(self, "COPYING", src=self.build_folder, dst=os.path.join(self.package_folder, "licenses"))
        fix_apple_shared_install_name(self)

    def package_info(self):
        self.cpp_info.set_property("cmake_target_name", "cppunit::cppunit")
        self.cpp_info.set_property("cmake_file_name", "CPPUnit")
        self.cpp_info.set_property("pkg_config_name", "cppunit")
        self.cpp_info.libs = ["cppunit"]
        self.cpp_info.includedirs = ["include"]