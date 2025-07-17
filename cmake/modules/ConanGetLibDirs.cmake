# Infomaniak kDrive - Desktop
# Copyright (C) 2023-2025 Infomaniak Network SA
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# -----------------------------------------------------------------------------
# This module helps find the library directories of the dependencies installed with Conan.
if(NOT CMAKE_BUILD_TYPE)
    message(FATAL_ERROR "You have to set -DCMAKE_BUILD_TYPE=<Debug|Release|RelWithDebInfo|...>.")
endif()

function(get_library_dirs prefix libname)
    find_package(${prefix} REQUIRED) # Find the package using the prefix, e.g., OpenSSL, xxHash, etc.
    # Let the variables named like openssl_LIB_DIRS_RELEASE or xxhash_LIB_DIRS_RELWITHDEBINFO become _openssl_LIB_DIRS or _xxhash_LIB_DIRS, independently of the build type.
    # This lets us use the same variable name in the CMakeLists.txt file, independently of the build type.
    string(TOUPPER "${CMAKE_BUILD_TYPE}" _BUILD_TYPE_UPPER)
    set(_${prefix}_LIB_DIRS "${${libname}_LIB_DIRS_${_BUILD_TYPE_UPPER}}" PARENT_SCOPE)
    message(STATUS "Using _${prefix}_LIB_DIRS (${libname}_LIB_DIRS_${_BUILD_TYPE_UPPER}) = ${_${prefix}_LIB_DIRS}")
endfunction()

function(get_include_dirs prefix libname)
    find_package(${prefix} REQUIRED)
    # Same for include directories as for library directories.
    string(TOUPPER "${CMAKE_BUILD_TYPE}" _BUILD_TYPE_UPPER) # cppunit_INCLUDE_DIRS_RELEASE
    set(_${prefix}_INCLUDE_DIRS "${${libname}_INCLUDE_DIRS_${_BUILD_TYPE_UPPER}}" PARENT_SCOPE)
    message(STATUS "Using _${prefix}_INCLUDE_DIRS (${libname}_INCLUDE_DIRS_${_BUILD_TYPE_UPPER}) = ${_${prefix}_INCLUDE_DIRS}")
endfunction()

function(get_include_and_library_dirs prefix libname)
    # This function combines the previous two functions to get both include and library directories.
    get_include_dirs(${prefix} ${libname})
    get_library_dirs(${prefix} ${libname})
    set(_${prefix}_INCLUDE_DIRS "${_${prefix}_INCLUDE_DIRS}" PARENT_SCOPE)
    set(_${prefix}_LIB_DIRS "${_${prefix}_LIB_DIRS}" PARENT_SCOPE)
endfunction()
