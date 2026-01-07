from os.path import join as pjoin
import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps
from conan.tools.files import copy, replace_in_file
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
        # Qt is private (headers=True, libs=False) because it uses cmake_find_mode="none"
        # and cannot be propagated via Conan - consumers must find Qt via find_package(Qt6)
        self.requires("qt/[>=6.2.3 <7.0.0]", headers=True, libs=False, visible=False)
        if self.settings.os == "Linux":
            # c-ares is required explicitly because Crashpad (Sentry's backend) uses it directly
            self.requires("c-ares/[>=1.27 <2]")
            self.requires("libcurl/8.10.1", options={"with_c_ares": True}) # Provide Curl with AsynchDNS (needed on linux)

    @property
    def forced_build_type(self):
        return "Release" # Force the build type to Release since we don't need to debug symbols

    def package_id(self):
        self.info.settings.rm_safe("build_type") # Since we force the build type to Release, we can remove it from the package ID to avoid creating multiple packages for the same configuration

    def source(self):
        git = Git(self)
        git.clone(url="https://github.com/getsentry/sentry-native.git", target=".", hide_url=False, args=["-b", str(self.version), "--recurse-submodules"])

    def _patch_sources(self):
        # Remove AsynchDNS component requirement as FindCURL doesn't detect it correctly with Conan
        # We ensure c-ares is enabled in libcurl requirements, so AsynchDNS is available
        replace_in_file(
            self,
            os.path.join(self.source_folder, "CMakeLists.txt"),
            "find_package(CURL REQUIRED COMPONENTS AsynchDNS)",
            "find_package(CURL REQUIRED)"
        )

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
        if self.settings.os == "Linux":
            cache_variables["SENTRY_TRANSPORT"] = "curl"
        return cache_variables

    def generate(self):
        tc = CMakeToolchain(self)
        for key, value in self._cache_variables().items():
            tc.cache_variables[key] = value
        tc.generate()

        # Generate CMakeDeps to allow Sentry's CMake to find libcurl and c-ares
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        self._patch_sources()
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

        # Remove auto-generated CMake config files (we use Conan's generated ones)
        cmake_dir = os.path.join(self.package_folder, "lib", "cmake", "sentry")
        import shutil
        shutil.rmtree(cmake_dir, ignore_errors=True)

        # Generate CMake module for crashpad_handler executable
        cmake_dir = os.path.join(self.package_folder, "lib", "cmake", "sentry")
        os.makedirs(cmake_dir, exist_ok=True)

        crashpad_handler_name = "crashpad_handler.exe" if self.settings.os == "Windows" else "crashpad_handler"
        crashpad_cmake_module = f'''# Generated by Conan for crashpad_handler executable

if(NOT TARGET sentry_crashpad::crashpad_handler)
    add_executable(sentry_crashpad::crashpad_handler IMPORTED)

    get_filename_component(_SENTRY_PKG_ROOT "${{CMAKE_CURRENT_LIST_DIR}}/../../.." ABSOLUTE)

    set_target_properties(sentry_crashpad::crashpad_handler PROPERTIES
        IMPORTED_LOCATION "${{_SENTRY_PKG_ROOT}}/bin/{crashpad_handler_name}"
        IMPORTED_LOCATION_RELEASE "${{_SENTRY_PKG_ROOT}}/bin/{crashpad_handler_name}"
        IMPORTED_LOCATION_DEBUG "${{_SENTRY_PKG_ROOT}}/bin/{crashpad_handler_name}"
        IMPORTED_LOCATION_RELWITHDEBINFO "${{_SENTRY_PKG_ROOT}}/bin/{crashpad_handler_name}"
    )

    unset(_SENTRY_PKG_ROOT)
endif()
'''
        with open(os.path.join(cmake_dir, "sentry-crashpad-handler.cmake"), "w") as f:
            f.write(crashpad_cmake_module)

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "sentry")
        self.cpp_info.set_property("cmake_find_mode", "both")

        self.cpp_info.set_property("cmake_build_modules", [pjoin(self.package_folder, "lib", "cmake", "sentry", "sentry-crashpad-handler.cmake")])

        comp_sentry = self.cpp_info.components["sentry"]
        comp_sentry.set_property("cmake_target_name", "sentry::sentry")
        comp_sentry.libs = ["sentry"]

        if self.settings.os == "Linux":
            # libcurl::curl should transitively provide c-ares, but we also add it explicitly
            comp_sentry.requires = ["libcurl::curl", "c-ares::cares"]
            comp_sentry.exelinkflags = ["-Wl,-E,--build-id=sha1"]
            comp_sentry.sharedlinkflags = ["-Wl,-E,--build-id=sha1"]
            comp_sentry.system_libs = [ "pthread", "dl" ]
        else:
            comp_sentry.requires = []

        if self.settings.os == "Windows":
            comp_sentry.system_libs = [ "dbghelp", "shlwapi", "version", "winhttp" ]

        if self.settings.os == "Macos":
            comp_sentry.frameworks = [ "CoreGraphics", "CoreText" ]

        if not self.options.shared:
            comp_sentry.defines = ["SENTRY_BUILD_STATIC"]

        comp_crashpad = self.cpp_info.components["crashpad_handler"]
        comp_crashpad.bindirs = ["bin"]
        comp_crashpad.libs = []  # No libraries, it's an executable