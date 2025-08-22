import os
import textwrap

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.apple import fix_apple_shared_install_name
from conan.tools.files import copy, save
from conan.tools.microsoft import unix_path


class OpenSSLMacos(ConanFile):
    name = "openssl-macos"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
    }
    default_options = {
        "shared": True,
    }
    exports_sources = "openssl_build.sh"


    def validate(self):
        if str(self.settings.os) != "Macos":
            raise ConanInvalidConfiguration("This recipe is only supported on macOS.")

    def config_option(self):
        if not self.options.shared:
            self.options.shared = True
            self.output.warning("This recipe only supports shared libraries on Apple platforms. The 'shared' option has been overwritten to True.")
        if self.settings.build_type != "Release":
            self.settings.build_type = "Release"
            self.output.warning("This recipe will build OpenSSL in Release mode regardless of the build type specified in the profile.")

    def build(self):
        zlib_cpp_info = self.dependencies["zlib"].cpp_info.aggregated_components()
        if not zlib_cpp_info.libs:
            raise ConanInvalidConfiguration("zlib is required to build OpenSSL, but no libraries were found in the zlib package.")
        zlib_include = zlib_cpp_info.includedirs[0]
        zlib_lib = zlib_cpp_info.libdirs[0]

        script = os.path.join(self.build_folder, "openssl_build.sh")

        self.run(f"/usr/bin/env bash {script} "
                 f"--version '{self.version}' "                        # to pass the version of OpenSSL
                 f"--build-folder '{self.build_folder}' "              # to pass the build folder
                 f"--zlib-include '{unix_path(self, zlib_include)}' "  # to pass the zlib include directory
                 f"--zlib-lib '{unix_path(self, zlib_lib)}' "          # to pass the zlib lib directory
                 f"--conan-arch '{self.settings.arch}'")               # to pass the conan profile defined of the architecture

    def package(self):
        copy(self, "*.h", src=os.path.join(self.build_folder, "openssl.multi", "include"), dst=os.path.join(self.package_folder, "include"))
        copy(self, "*.dylib", src=os.path.join(self.build_folder, "openssl.multi", "lib"), dst=os.path.join(self.package_folder, "lib"))
        copy(self, "*.pc", src=os.path.join(self.build_folder, "openssl.x86_64"), dst=self.package_folder)
        fix_apple_shared_install_name(self)

        self._create_cmake_module_variables(
            os.path.join(self.package_folder, self._module_file_rel_path)
        )

    def requirements(self):
        self.requires("zlib/[>=1.2.11 <2]", transitive_headers=True, options={"shared": False})


    def _create_cmake_module_variables(self, module_file):
        content = textwrap.dedent("""\
            set(OPENSSL_FOUND TRUE)
            if(DEFINED OpenSSL_INCLUDE_DIR)
                set(OPENSSL_INCLUDE_DIR ${OpenSSL_INCLUDE_DIR})
            endif()
            if(DEFINED OpenSSL_Crypto_LIBS)
                set(OPENSSL_CRYPTO_LIBRARY ${OpenSSL_Crypto_LIBS})
                set(OPENSSL_CRYPTO_LIBRARIES ${OpenSSL_Crypto_LIBS}
                                             ${OpenSSL_Crypto_DEPENDENCIES}
                                             ${OpenSSL_Crypto_FRAMEWORKS}
                                             ${OpenSSL_Crypto_SYSTEM_LIBS})
            elseif(DEFINED openssl_OpenSSL_Crypto_LIBS_%(config)s)
                set(OPENSSL_CRYPTO_LIBRARY ${openssl_OpenSSL_Crypto_LIBS_%(config)s})
                set(OPENSSL_CRYPTO_LIBRARIES ${openssl_OpenSSL_Crypto_LIBS_%(config)s}
                                             ${openssl_OpenSSL_Crypto_DEPENDENCIES_%(config)s}
                                             ${openssl_OpenSSL_Crypto_FRAMEWORKS_%(config)s}
                                             ${openssl_OpenSSL_Crypto_SYSTEM_LIBS_%(config)s})
            endif()
            if(DEFINED OpenSSL_SSL_LIBS)
                set(OPENSSL_SSL_LIBRARY ${OpenSSL_SSL_LIBS})
                set(OPENSSL_SSL_LIBRARIES ${OpenSSL_SSL_LIBS}
                                          ${OpenSSL_SSL_DEPENDENCIES}
                                          ${OpenSSL_SSL_FRAMEWORKS}
                                          ${OpenSSL_SSL_SYSTEM_LIBS})
            elseif(DEFINED openssl_OpenSSL_SSL_LIBS_%(config)s)
                set(OPENSSL_SSL_LIBRARY ${openssl_OpenSSL_SSL_LIBS_%(config)s})
                set(OPENSSL_SSL_LIBRARIES ${openssl_OpenSSL_SSL_LIBS_%(config)s}
                                          ${openssl_OpenSSL_SSL_DEPENDENCIES_%(config)s}
                                          ${openssl_OpenSSL_SSL_FRAMEWORKS_%(config)s}
                                          ${openssl_OpenSSL_SSL_SYSTEM_LIBS_%(config)s})
            endif()
            if(DEFINED OpenSSL_LIBRARIES)
                set(OPENSSL_LIBRARIES ${OpenSSL_LIBRARIES})
            endif()
            if(DEFINED OpenSSL_VERSION)
                set(OPENSSL_VERSION ${OpenSSL_VERSION})
            endif()
        """% {"config":str(self.settings.build_type).upper()})
        save(self, module_file, content)

    @property
    def _module_subfolder(self):
        return os.path.join("lib", "cmake")

    @property
    def _module_file_rel_path(self):
        return os.path.join(self._module_subfolder,
                            f"conan-official-{self.name}-variables.cmake")

    def package_info(self):
        self.cpp_info.set_property("cmake_target_name", "openssl::openssl")
        self.cpp_info.set_property("cmake_file_name", "OpenSSL")
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("pkg_config_name", "openssl")

        if self.options.shared:
            self.cpp_info.components["ssl"].libs = ["ssl.3"]
            self.cpp_info.components["crypto"].libs = ["crypto.3"]
        else:
            self.cpp_info.components["ssl"].libs = ["ssl"]
            self.cpp_info.components["crypto"].libs = ["crypto"]

        self.cpp_info.components["ssl"].requires = ["crypto"]

        self.cpp_info.components["ssl"].includedirs = ["include"]
        self.cpp_info.components["ssl"].set_property("cmake_target_name", "OpenSSL::SSL")
        self.cpp_info.components["ssl"].set_property("pkg_config_name", "libssl")

        self.cpp_info.components["crypto"].includedirs = ["include"]
        self.cpp_info.components["crypto"].set_property("cmake_target_name", "OpenSSL::Crypto")
        self.cpp_info.components["crypto"].set_property("pkg_config_name", "libcrypto")

        self.cpp_info.set_property("cmake_build_modules", [self._module_file_rel_path])

    def package_id(self):
        self.info.settings.rm_safe("compiler")
        self.info.settings.rm_safe("build_type")
        self.info.options.rm_safe("shared")
        self.info.requires.clear()