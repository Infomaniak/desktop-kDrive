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
        # Update the hash together with the version in config.yml.
        get(
            self,
            f"https://github.com/hrantzsch/keychain/archive/refs/tags/v{self.version}.tar.gz",
            sha256="9a9fc5aaa94d03180f1c3735f93d1ebcf9d2925fc915306b8476abc17a3906a9",
            strip_root=True,
        )

    def system_requirements(self):
        """Validate required Linux system packages for keychain and fail if they are missing."""
        # Upstream finds glib and libsecret through pkg-config without REQUIRED. The glib development
        # packages already depend on pkg-config, except on Arch where it is part of base-devel.
        if self.settings.os != "Linux":
            return

        managers = [
            (package_manager.Apt, ["libglib2.0-dev", "libsecret-1-dev"]),  # Debian/Ubuntu/Mint/Raspbian
            (package_manager.Yum, ["glib2-devel", "libsecret-devel"]),  # Amazon Linux
            (package_manager.Dnf, ["glib2-devel", "libsecret-devel"]),  # Fedora/RHEL/CentOS/Rocky/Alma
            (package_manager.PacMan, ["pkgconf", "glib2", "libsecret"]),  # Arch/Manjaro
            (package_manager.Zypper, ["glib2-devel", "libsecret-devel"]),  # openSUSE/SLES
            (package_manager.Apk, ["glib-dev", "libsecret-dev"]),  # Alpine
        ]

        for manager_class, packages in managers:
            missing = manager_class(self).check(packages)
            if missing is None:
                # This manager is not active on the current system.
                continue

            if missing:
                raise ConanInvalidConfiguration(
                    f"Missing Linux system packages required by keychain recipe: {', '.join(missing)}. "
                    "Install them manually and retry."
                )

            self.output.info(f"All required Linux system packages for keychain are installed: {', '.join(packages)}.")
            return

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
        # Keep the namespaced target exposed by the ConanCenter recipe: a namespaced name makes CMake
        # fail at configure time if the target is missing, instead of silently linking -lkeychain.
        self.cpp_info.set_property("cmake_file_name", "keychain")
        self.cpp_info.set_property("cmake_target_name", "keychain::keychain")
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
