from os.path import join as pjoin
import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, CMakeToolchain
from conan.tools.files import copy
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
    topics = ("crashpad", "error-reporting", "crash-reporting")

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"

    options = {"shared": [True, False]}
    default_options = {"shared": True}

    def validate(self):
        if self.settings.os not in ("Macos", "Linux", "Windows"):
            raise ConanInvalidConfiguration(
                f"This recipe does not support yet {self.settings.os}."
            )
        if self.settings.os == "Linux" and str(self.settings.compiler) not in ("gcc", "clang"):
            raise ConanInvalidConfiguration(
                "On Linux, this recipe requires GCC or Clang because the Sentry/Crashpad build uses GCC/Clang-specific preprocessor extensions such as #include_next."
            )

    def requirements(self):
        if self.settings.os == "Linux" and str(self.settings.arch).startswith("arm"): # linux arm64
            self.requires("qt/6.7.3")
        else:
            self.requires("qt/6.2.3")

    @property
    def forced_build_type(self):
        return "Release" # Force the build type to Release since we don't need to debug symbols

    def package_id(self):
        self.info.settings.rm_safe("build_type") # Since we force the build type to Release, we can remove it from the package ID to avoid creating multiple packages for the same configuration

    def source(self):
        git = Git(self)
        git.clone(url="https://github.com/getsentry/sentry-native.git", target=".", hide_url=False, args=["-b", str(self.version), "--recurse-submodules"])

    def _cache_variables(self):
        qt = self.dependencies["qt"]
        if qt is None:
            raise ConanInvalidConfiguration("The 'qt' dependency is required for the 'sentry' recipe.")
        cache_variables = {
            "SENTRY_INTEGRATION_QT": "YES",
            "SENTRY_BACKEND": "crashpad",
            "CMAKE_PREFIX_PATH": qt.package_folder,
            "SENTRY_BUILD_TESTS": "OFF",
            "SENTRY_BUILD_EXAMPLES": "OFF",
            "SENTRY_BUILD_SHARED_LIBS": "ON" if self.options.shared else "OFF",
        }
        if self.settings.os != "Windows":
            cache_variables["CMAKE_BUILD_TYPE"] = self.forced_build_type
        return cache_variables

    def generate(self):
        tc = CMakeToolchain(self)
        for key, value in self._cache_variables().items():
            tc.cache_variables[key] = value
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(variables=self._cache_variables())
        cmake.build(target="sentry")

    def package(self):
        copy(self, "LICENSE*", src=self.source_folder, dst=pjoin(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        if self.settings.os == "Macos":
            import shutil
            dsym_path = os.path.join(self.package_folder, "lib", "libsentry.dylib.dSYM")
            if os.path.exists(dsym_path):
                shutil.rmtree(dsym_path)

        # Remove CMake config files
        cmake_dir = os.path.join(self.package_folder, "lib", "cmake", "sentry")
        for fname in ["sentry-targets.cmake", "sentry-targets-release.cmake", "sentry-config.cmake", "sentry-config-version.cmake"]:
            fpath = os.path.join(cmake_dir, fname)
            if os.path.exists(fpath):
                os.remove(fpath)

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "sentry")
        self.cpp_info.set_property("cmake_find_mode", "both")

        comp_sentry = self.cpp_info.components["sentry"]
        comp_sentry.set_property("cmake_target_name", "sentry::sentry")
        comp_sentry.libs = ["sentry"]

        if self.settings.os == "Linux":
            comp_sentry.exelinkflags = ["-Wl,-E,--build-id=sha1"]
            comp_sentry.sharedlinkflags = ["-Wl,-E,--build-id=sha1"]
            comp_sentry.system_libs = [ "pthread", "dl" ]

        if self.settings.os == "Windows":
            comp_sentry.system_libs = [ "dbghelp", "shlwapi", "version", "winhttp" ]

        if self.settings.os == "Macos":
            comp_sentry.frameworks = [ "CoreGraphics", "CoreText" ]

        if not self.options.shared:
            comp_sentry.defines = ["SENTRY_BUILD_STATIC"]

        self.cpp_info.set_property("cmake_build_modules", [pjoin(self.package_folder, "lib", "cmake", "sentry", "sentry_crashpad-targets.cmake")])