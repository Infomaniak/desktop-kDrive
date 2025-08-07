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
        if self.settings.os == "Linux" and self.settings.compiler != "gcc":
            raise ConanInvalidConfiguration(
                f"Sentry Crashpad on Linux currently only supports GCC due to use of GCC-specific extensions (e.g., #include_next)."
            )

    def requirements(self):
        self.requires("qt/6.2.3")

    @property
    def build_type(self):
        return "RelWithDebInfo" # Force the build type to RelWithDebInfo since we don't need to debug Sentry

    def source(self):
        git = Git(self)
        git.clone(url="https://github.com/getsentry/sentry-native.git", target=".", hide_url=False, args=["-b", f"{self.version}", "--recurse-submodules"])

    def _cache_variables(self):
        qt = self.dependencies["qt"]
        if qt is None:
            raise ConanInvalidConfiguration("The 'qt' dependency is required for the 'sentry' recipe.")
        cache_variables = {
            "CMAKE_BUILD_TYPE": self.build_type,
            "SENTRY_INTEGRATION_QT": "YES",
            "SENTRY_BACKEND": "crashpad",
            "CMAKE_PREFIX_PATH": qt.package_folder,
            "SENTRY_BUILD_TESTS": "OFF",
            "SENTRY_BUILD_EXAMPLES": "OFF",
            "SENTRY_BUILD_SHARED_LIBS": "ON" if self.options.shared else "OFF",
        }
        return cache_variables

    def generate(self):
        tc = CMakeToolchain(self)
        for key, value in self._cache_variables().items():
            tc.cache_variables[key] = value
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(variables=self._cache_variables())
        cmake.build(build_type=self.build_type if self.settings.os == "Windows" else None, target="sentry")

    def package(self):
        cmake = CMake(self)
        cmake.install(build_type=self.build_type if self.settings.os == "Windows" else None)
        if self.settings.os == "Macos":
            rmdir(self, pjoin(self.package_folder, "lib", "libsentry.dylib.dSYM"))
        if self.settings.os == "Windows":
            rm(self, "*.pdb", self.package_folder, recursive=True)



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