import os
from collections import namedtuple

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import apply_conandata_patches, copy, export_conandata_patches, get, rm, rmdir
from conan.tools.microsoft import is_msvc, is_msvc_static_runtime, msvc_runtime_flag
from conan.tools.scm import Version, Git

required_conan_version = ">=2.1"


class PocoConan(ConanFile):
    name = "poco"
    version = "1.13.3"
    description = (
        "Modern, powerful open source C++ class libraries for building "
        "network- and internet-based applications that run on desktop, server, "
        "mobile and embedded systems."
    )
    license = "BSL-1.0"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://pocoproject.org"
    topics = ("building", "networking", "server", "mobile", "embedded")
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "enable_fork": [True, False],
        "enable_active_record": [True, False, "deprecated"],
        "log_debug": [True, False],
        "with_sql_parser": [True, False],
        "comp_foundation_sharedlibrary_debug_suffix": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
        "enable_fork": True,
        "enable_active_record": "deprecated",
        "log_debug": False,
        "with_sql_parser": False,
        "comp_foundation_sharedlibrary_debug_suffix": True,
    }

    _PocoComponent = namedtuple("_PocoComponent", ("option",                    "default_option",   "dependencies", "external_dependencies", "is_lib"))
    _poco_component_tree = {
        "Foundation":           _PocoComponent(None, "Foundation",              [],                 ["pcre::pcre", "zlib::zlib"], True),
        "Crypto":               _PocoComponent("enable_crypto",                 True,               ["Foundation"], ["openssl::openssl"], True),
        "JSON":                 _PocoComponent("enable_json",                   True,               ["Foundation"], [], True),
        "Net":                  _PocoComponent("enable_net",                    True,               ["Foundation"], [], True),
        "NetSSL":               _PocoComponent("enable_netssl",                 True,               ["Crypto", "Util", "Net"], [], True),
        "Util":                 _PocoComponent("enable_util",                   True,               ["Foundation", "XML", "JSON"], [], True),
        "XML":                  _PocoComponent("enable_xml",                    True,               ["Foundation"], ["expat::expat"], True),

        "mod_poco":             _PocoComponent("enable_apacheconnector",        False,              ["Util", "Net"], ["apr::apr", "apr-util::apr-util"], False),
        "CppParser":            _PocoComponent("enable_cppparser",              False,              ["Foundation"], [], False),
        # "CppUnit":            _PocoComponent("enable_cppunit",                False,              ["Foundation"], [], False)),
        "Data":                 _PocoComponent("enable_data",                   False,              ["Foundation"], [], True),
        "DataMySQL":            _PocoComponent("enable_data_mysql",             False,              ["Data"], ["libmysqlclient::libmysqlclient"], True),
        "DataODBC":             _PocoComponent("enable_data_odbc",              False,              ["Data"], [], True), # requires odbc but conditional, see package_info()
        "DataPostgreSQL":       _PocoComponent("enable_data_postgresql",        False,              ["Data"], ["libpq::libpq"], True),
        "DataSQLite":           _PocoComponent("enable_data_sqlite",            False,              ["Data"], ["sqlite3::sqlite3"], True),
        "Encodings":            _PocoComponent("enable_encodings",              False,              ["Foundation"], [], True),
        # "EncodingsCompiler":  _PocoComponent("enable_encodingscompiler",      False,              ["Net", "Util"], [], False),
        "JWT":                  _PocoComponent("enable_jwt",                    False,              ["JSON", "Crypto"], [], True),
        "MongoDB":              _PocoComponent("enable_mongodb",                False,              ["Net"], [], True),
        "NetSSLWin":            _PocoComponent("enable_netssl_win",             False,              ["Net", "Util"], [], True),
        "PDF":                  _PocoComponent("enable_pdf",                    False,              ["XML", "Util"], [], True),
        "PageCompiler":         _PocoComponent("enable_pagecompiler",           False,              ["Net", "Util"], [], False),
        "File2Page":            _PocoComponent("enable_pagecompiler_file2page", False,              ["Net", "Util", "XML", "JSON"], [], False),
        "PocoDoc":              _PocoComponent("enable_pocodoc",                False,              ["Util", "XML", "CppParser"], [], False),
        "Redis":                _PocoComponent("enable_redis",                  False,              ["Net"], [], True),
        "SevenZip":             _PocoComponent("enable_sevenzip",               False,              ["Util", "XML"], [], True),
        "Zip":                  _PocoComponent("enable_zip",                    False,              ["Util", "XML"], [], True),
        "ActiveRecord":         _PocoComponent("enable_activerecord",           False,              ["Foundation", "Data"], [], True),
        "ActiveRecordCompiler": _PocoComponent("enable_activerecord_compiler",  False,              ["Util", "XML"], [], False),
        "Prometheus":           _PocoComponent("enable_prometheus",             False,              ["Foundation", "Net"], [], True),
    }

    for comp in _poco_component_tree.values():
        if comp.option:
            options[comp.option] = [True, False]
            default_options[comp.option] = comp.default_option
    del comp

    @property
    def _is_mingw(self):
        return self.settings.os == "Windows" and self.settings.compiler == "gcc"

    def export_sources(self):
        export_conandata_patches(self)

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
            del self.options.enable_fork
        else:
            del self.options.enable_netssl_win
        if self.settings.build_type != "Debug":
            del self.options.comp_foundation_sharedlibrary_debug_suffix

    def configure(self):
        if self.options.enable_active_record != "deprecated":
            self.output.warning("enable_active_record option is deprecated, use 'enable_activerecord' instead")
        if self.options.shared:
            self.options.rm_safe("fPIC")
        if not self.options.enable_xml:
            util_dependencies = self._poco_component_tree["Util"].dependencies
            self._poco_component_tree["Util"] = self._poco_component_tree["Util"]._replace(dependencies = [x for x in util_dependencies if x != "XML"])
        if not self.options.enable_json:
            util_dependencies = self._poco_component_tree["Util"].dependencies
            self._poco_component_tree["Util"] = self._poco_component_tree["Util"]._replace(dependencies = [x for x in util_dependencies if x != "JSON"])

        foundation_external_dependencies = self._poco_component_tree["Foundation"].external_dependencies
        self._poco_component_tree["Foundation"] = self._poco_component_tree["Foundation"]._replace(external_dependencies = list(map(lambda x: 'pcre2::pcre2' if x == 'pcre::pcre' else x, foundation_external_dependencies)))


    def layout(self):
        cmake_layout(self, src_folder="src")

    # Crypto, Foundation, JSON, Net, NetSSL, Util, XML
    def requirements(self):
        self.requires("pcre2/[>=10.42 <11]")
        self.requires("zlib/[>=1.2.11 <2]", transitive_headers=True, options={"shared": self.options.shared})
        if self.options.enable_xml:
            self.requires("expat/[>=2.6.2 <3]", transitive_headers=True)
        if self.options.enable_netssl or self.options.enable_crypto:
            self.requires("openssl/3.2.4", options={"shared": self.options.shared})

    def package_id(self):
        del self.info.options.enable_active_record
        del self.info.options.log_debug

    def validate_build(self):
        check_min_cppstd(self, 17)

    def validate(self):
        #  1.13.3: https://github.com/pocoproject/poco/blob/d6bd48a94c5f03e3c69cac1b024fdad5120e3a7b/Foundation/CMakeLists.txt#L125-L128
        #  1.14.2: https://github.com/pocoproject/poco/blob/96d182a99303fb068575294b36f0cc20da2e7b25/Foundation/CMakeLists.txt#L130
        check_min_cppstd(self, 14)

        if is_msvc(self) and self.options.shared and is_msvc_static_runtime(self):
            raise ConanInvalidConfiguration("Cannot build shared poco libraries with MT(d) runtime")
        for compopt in self._poco_component_tree.values():
            if not compopt.option:
                continue
            if self.options.get_safe(compopt.option, False):
                for compdep in compopt.dependencies:
                    if not self._poco_component_tree[compdep].option:
                        continue
                    if not self.options.get_safe(self._poco_component_tree[compdep].option, False):
                        raise ConanInvalidConfiguration(f"option {compopt.option} requires also option {self._poco_component_tree[compdep].option}")
        if self.options.enable_netssl and self.options.get_safe("enable_netssl_win", False):
            raise ConanInvalidConfiguration("Conflicting enable_netssl[_win] settings")

    def source(self):
        git = Git(self)
        git.clone(url="https://github.com/pocoproject/poco.git", target=".", hide_url=False, args=["-b", f"poco-{self.version}-release"])

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_BUILD_TYPE"] = self.settings.build_type
        for comp in self._poco_component_tree.values():
            if comp.option:
                tc.variables[comp.option.upper()] = self.options.get_safe(comp.option, False)
        tc.variables["POCO_UNBUNDLED"] = True
        tc.variables["CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP"] = True
        if is_msvc(self):
            tc.variables["POCO_MT"] = is_msvc_static_runtime(self)
        if self.options.get_safe("with_sql_parser", None) is False:
            tc.variables["POCO_DATA_NO_SQL_PARSER"] = True
        # Disable automatic linking on MSVC
        tc.preprocessor_definitions["POCO_NO_AUTOMATIC_LIBS"] = "1"
        # Picked up from conan v1 CMake wrapper, don't know the rationale
        tc.preprocessor_definitions["XML_DTD"] = "1"
        # Disable SharedLibrary::suffix() including "d" as part of the platform-specific filename suffix
        if not self.options.get_safe("comp_foundation_sharedlibrary_debug_suffix", True):
            tc.preprocessor_definitions["POCO_NO_SHARED_LIBRARY_DEBUG_SUFFIX"] = "1"
        tc.generate()

        deps = CMakeDeps(self)
        deps.set_property("expat", "cmake_file_name", "EXPAT")
        deps.set_property("expat", "cmake_target_name", "EXPAT::EXPAT")
        deps.set_property("expat", "cmake_find_mode", "config")
        deps.set_property("pcre2::pcre2-8", "cmake_target_name", "Pcre2::Pcre2")
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(variables={
            "OPENSSL_FOUND": "TRUE" # Fix FindOpenSSL.cmake
        })
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "cmake"))
        rm(self, "*.pdb", os.path.join(self.package_folder, "bin"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "Poco")
        self.cpp_info.set_property("cmake_target_name", "Poco::Poco")

        suffix = msvc_runtime_flag(self).lower() \
            if is_msvc(self) and not self.options.shared \
            else ("d" if self.settings.build_type == "Debug" else "")

        for compname, comp in self._poco_component_tree.items():
            if comp.option is None or self.options.get_safe(comp.option):
                conan_component = f"poco_{compname.lower()}"
                requires = [f"poco_{dependency.lower()}" for dependency in comp.dependencies] + comp.external_dependencies
                self.cpp_info.components[conan_component].set_property("cmake_target_name", f"Poco::{compname}")
                self.cpp_info.components[conan_component].set_property("cmake_file_name", compname)
                if comp.is_lib:
                    self.cpp_info.components[conan_component].libs = [f"Poco{compname}{suffix}"]
                self.cpp_info.components[conan_component].requires = requires

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["poco_foundation"].system_libs.extend(["pthread", "dl", "rt"])

        if self.options.log_debug:
            self.cpp_info.components["poco_foundation"].defines.append("POCO_LOG_DEBUG")

        if is_msvc(self):
            self.cpp_info.components["poco_foundation"].defines.append("POCO_NO_AUTOMATIC_LIBS")
        if not self.options.shared:
            self.cpp_info.components["poco_foundation"].defines.append("POCO_STATIC=ON")
            if self.settings.os == "Windows":
                self.cpp_info.components["poco_foundation"].system_libs.extend(["ws2_32", "iphlpapi", "crypt32"])
        if self.options.enable_net:
            if not self.options.shared and self._is_mingw:
                self.cpp_info.components["poco_net"].system_libs.extend(["mswsock"])
        self.cpp_info.components["poco_foundation"].defines.append("POCO_UNBUNDLED")
        if self.options.enable_util:
            if not self.options.enable_json:
                self.cpp_info.components["poco_util"].defines.append("POCO_UTIL_NO_JSONCONFIGURATION")
            if not self.options.enable_xml:
                self.cpp_info.components["poco_util"].defines.append("POCO_UTIL_NO_XMLCONFIGURATION")