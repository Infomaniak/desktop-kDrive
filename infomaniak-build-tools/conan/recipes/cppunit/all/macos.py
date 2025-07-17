import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.apple import fix_apple_shared_install_name
from conan.tools.build import stdcpp_library
from conan.tools.files import rmdir, rm, copy


class CPPUnitMacOSConan(ConanFile):
    name = "cppunit"
    version = "1.15.1"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
    }
    default_options = {
        "shared": True,
    }
    _script_name = "cppunit_macos_build.sh"

    exports_sources = _script_name

    def validate(self):
        if not self.settings.os == "Macos":
            raise ConanInvalidConfiguration("This custom cppunit recipe is only supported on macOS.")

    def build(self):
        script = os.path.join(self.build_folder, self._script_name)

        self.run(f"chmod +x {script}")
        self.run(f"bash {script} --build-folder {self.build_folder} {'--shared' if self.options.shared else '--static'} --version {self.version} --package-folder {self.package_folder}")

    def package(self):
        copy(self, "COPYING", src=self.build_folder, dst=os.path.join(self.package_folder, "licenses"))
        rm(self, "*.la", os.path.join(self.package_folder, "lib"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))

        fix_apple_shared_install_name(self)

    def package_info(self):
        self.cpp_info.set_property("pkg_config_name", "cppunit")
        self.cpp_info.libs = ["cppunit"]

        if not self.options.shared:
            libcxx = stdcpp_library(self)
            if libcxx:
                self.cpp_info.system_libs.append(libcxx)