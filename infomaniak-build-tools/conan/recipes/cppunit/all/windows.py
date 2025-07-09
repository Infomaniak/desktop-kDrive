import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import stdcpp_library
from conan.tools.env import VirtualBuildEnv
from conan.tools.files import copy, get, rename, rm, rmdir, replace_in_file
from conan.tools.gnu import Autotools, AutotoolsToolchain
from conan.tools.layout import basic_layout
from conan.tools.microsoft import check_min_vs, is_msvc, unix_path

required_conan_version = ">=1.57.0"


class CppunitConan(ConanFile):
    name = "cppunit"
    description = (
        "CppUnit is the C++ port of the famous JUnit framework for unit testing. "
        "Test output is in XML for automatic testing and GUI based for supervised tests."
    )
    topics = ("unit-test", "tdd")
    license = " LGPL-2.1-or-later"
    homepage = "https://freedesktop.org/wiki/Software/cppunit/"
    url = "https://github.com/conan-io/conan-center-index"
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

    def validate(self):
        if not self.settings.os == "Windows":
            raise ConanInvalidConfiguration("This custom cppunit recipe is only supported on Windows.")

    @property
    def _settings_build(self):
        return getattr(self, "settings_build", self.settings)

    def config_options(self):
        del self.options.fPIC

    def layout(self):
        basic_layout(self, src_folder="src")

    def build_requirements(self):
        self.win_bash = True
        if not self.conf.get("tools.microsoft.bash:path", check_type=str):
            self.tool_requires("msys2/cci.latest")
        if is_msvc(self):
            self.tool_requires("automake/1.16.5")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

        mafile_in = os.path.join(self.source_folder, "Makefile.in")
        replace_in_file(self, mafile_in, "SUBDIRS = src include examples doc", "SUBDIRS = src include")

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        tc = AutotoolsToolchain(self)
        if is_msvc(self):
            tc.extra_cxxflags.append("-EHsc")
            if check_min_vs(self, "180", raise_invalid=False):
                tc.extra_cflags.append("-FS")
                tc.extra_cxxflags.append("-FS")
        yes_no = lambda v: "yes" if v else "no"
        tc.configure_args.extend([
            "--enable-debug={}".format(yes_no(self.settings.build_type == "Debug")),
            "--enable-doxygen=no",
            "--enable-dot=no",
            "--disable-werror",
            "--enable-html-docs=no",
        ])
        env = tc.environment()
        if is_msvc(self):
            compile_wrapper = unix_path(self, self.conf.get("user.automake:compile-wrapper", check_type=str))
            ar_wrapper = unix_path(self, self.conf.get("user.automake:lib-wrapper", check_type=str))
            env.define("CC", f"{compile_wrapper} cl -nologo")
            env.define("CXX", f"{compile_wrapper} cl -nologo")
            env.define("LD", "link -nologo")
            env.define("AR", f"{ar_wrapper} \"lib -nologo\"")
            env.define("NM", "dumpbin -symbols")
            env.define("OBJDUMP", ":")
            env.define("RANLIB", ":")
            env.define("STRIP", ":")
        tc.generate(env)

    def build(self):
        autotools = Autotools(self)
        autotools.configure()

        self.output.info("Patching Makefiles to remove -Werror")
        for root, _, files in os.walk(self.build_folder):
            if "Makefile" in files:
                makefile_path = os.path.join(root, "Makefile")
                replace_in_file(self, makefile_path, "-Werror", "", strict=False)

        autotools.make()

    def package(self):
        copy(self, "COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        autotools = Autotools(self)
        self.output.info("Install")
        autotools.install()
        self.output.info("Install finished")
        if is_msvc(self) and self.options.shared:
            rename(self, os.path.join(self.package_folder, "lib", "cppunit.dll.lib"),
                   os.path.join(self.package_folder, "lib", "cppunit.lib"))
            old_dll_path = os.path.join(self.package_folder, "bin", f"cppunit-{self.version}.dll")
            new_dll_path = os.path.join(self.package_folder, "bin", "cppunit.dll")
            if os.path.exists(old_dll_path):
                rename(self, old_dll_path, new_dll_path)
        rm(self, "*.la", os.path.join(self.package_folder, "lib"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.set_property("pkg_config_name", "cppunit")
        self.cpp_info.libs = ["cppunit"]
        if self.options.shared:
            self.cpp_info.defines.append("CPPUNIT_DLL")
        else:
            libcxx = stdcpp_library(self)
            if libcxx:
                self.cpp_info.system_libs.append(libcxx)