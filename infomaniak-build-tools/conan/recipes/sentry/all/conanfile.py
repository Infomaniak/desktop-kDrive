from os.path import join as pjoin

from conan import ConanFile, Version
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, CMakeToolchain
from conan.tools.files import rmdir, rm
from conan.tools.scm import Git

required_conan_version = ">=2"

class SentryNativeConan(ConanFile):
    name = "sentry"
    description = (
        "The Sentry Native SDK is an error and crash reporting client for native "
        "applications, optimized for C and C++. Sentry allows to add tags, "
        "breadcrumbs and arbitrary custom context to enrich error reports."
    )
    license = "MIT"
    topics = ("breakpad", "crashpad", "error-reporting", "crash-reporting")

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"

    options = {"shared": [True, False]}
    default_options = {"shared": True}

    def validate(self):
        if self.settings.os not in ("Macos", "Linux", "Windows"):
            raise ConanInvalidConfiguration(
                f"This recipe does not support yet {self.settings.os}."
            )

    def requirements(self):
        self.requires("qt/6.2.3")

    @property
    def build_type(self):
        return "Release" # Force the build type to Release since we don't need to debug Sentry

    def source(self):
        git = Git(self)
        git.clone(url="https://github.com/getsentry/sentry-native.git", target=".", hide_url=False, args=["-b", f"{self.version}", "--recurse-submodules"])

    def _cache_variables(self, qt_package_folder):
        cache_variables = {
            "CMAKE_BUILD_TYPE": "RelWithDebInfo",
            "SENTRY_INTEGRATION_QT": "YES",
            "SENTRY_BACKEND": "breakpad" if Version(self.version) < "0.7.0" else "crashpad",
            "CMAKE_PREFIX_PATH": qt_package_folder,
            "SENTRY_BUILD_TESTS": "OFF",
            "SENTRY_BUILD_EXAMPLES": "OFF",
            "SENTRY_BUILD_SHARED_LIBS": "ON" if self.options.shared else "OFF",
        }
        return cache_variables

    def generate(self):
        tc = CMakeToolchain(self)
        qt_package_folder = self.dependencies["qt"].package_folder
        for key, value in self._cache_variables(qt_package_folder).items():
            tc.cache_variables[key] = value
        tc.generate()

    def build(self):
        qt_package_folder = self.dependencies["qt"].package_folder

        cmake = CMake(self)
        cmake.configure(variables=self._cache_variables(qt_package_folder))
        cmake.build(build_type="RelWithDebInfo" if self.settings.os == "Windows" else None, target="sentry")

    def package(self):
        cmake = CMake(self)
        cmake.install(build_type="RelWithDebInfo" if self.settings.os == "Windows" else None)
        if self.settings.os == "Macos":
            rmdir(self, pjoin(self.package_folder, "lib", "libsentry.dylib.dSYM"))
        if self.settings.os == "Windows":
            rm(self, "*.pdb", self.package_folder, recursive=True)



    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "sentry")
        # Here we use the same hack we used for Qt to ensure the link between the Qt package and the Sentry package is done correctly.
        self.cpp_info.set_property("cmake_build_modules", [ pjoin(self.package_folder, "lib", "cmake", "sentry", "sentry-config.cmake") ])
        self.cpp_info.set_property("cmake_find_mode", "config")
        for env in ( self.buildenv_info, self.runenv_info ):
            env.prepend_path("PATH", pjoin(self.package_folder, "bin"))
            env.prepend_path("CMAKE_PREFIX_PATH", self.package_folder)