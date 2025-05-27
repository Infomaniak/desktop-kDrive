from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.files import copy
from conan.tools.apple import fix_apple_shared_install_name, is_apple_os
import os

class OpenSSLUniversalConan(ConanFile):
    name = "openssl-universal"
    version = "3.2.4"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
    }
    default_options = {
        "shared": True,
    }
    exports_sources = "include/*", "lib/*"


    def validate(self):
        if not is_apple_os(self):
            raise ConanInvalidConfiguration("openssl-universal is only supported on Apple platforms (macOS, iOS, etc.)")

    def package(self):
        copy(self, "*.h", src="include", dst=os.path.join(self.package_folder, "include"))

        if self.options.shared:
            copy(self, "*.dylib", src="lib", dst=os.path.join(self.package_folder, "lib"))
            fix_apple_shared_install_name(self)  # ← ici
        else:
            copy(self, "*.a", src="lib", dst=os.path.join(self.package_folder, "lib"))

    def requirements(self):
        self.requires("zlib/[>=1.2.11 <2]")

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "OpenSSL")
        self.cpp_info.set_property("pkg_config_name", "openssl")

        if self.options.shared:
            self.cpp_info.components["ssl"].libs = ["ssl.3"]
            self.cpp_info.components["crypto"].libs = ["crypto.3"]
        else:
            self.cpp_info.components["ssl"].libs = ["ssl"]
            self.cpp_info.components["crypto"].libs = ["crypto"]

        self.cpp_info.components["ssl"].requires = ["crypto"]

        self.cpp_info.components["crypto"].set_property("cmake_target_name", "OpenSSL::Crypto")
        self.cpp_info.components["ssl"].set_property("cmake_target_name", "OpenSSL::SSL")

        self.cpp_info.components["crypto"].set_property("pkg_config_name", "libcrypto")
        self.cpp_info.components["ssl"].set_property("pkg_config_name", "libssl")

        self.cpp_info.components["ssl"].includedirs = ["include"]
        self.cpp_info.components["crypto"].includedirs = ["include"]
