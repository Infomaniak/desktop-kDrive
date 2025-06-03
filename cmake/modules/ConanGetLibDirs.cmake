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
# This module generates the OpenSSL library directories based on the build type.
if(NOT CMAKE_BUILD_TYPE)
    message(FATAL_ERROR "You have to set -DCMAKE_BUILD_TYPE=<Debug|Release|RelWithDebInfo|...>.")
endif()

function(get_library_dirs prefix libname)
    find_package(${prefix} REQUIRED)

    string(TOUPPER "${CMAKE_BUILD_TYPE}" _BUILD_TYPE_UPPER)
    set(var_name "${libname}_LIB_DIRS_${_BUILD_TYPE_UPPER}")                            # e.g., openssl_LIB_DIRS_RELEASE or xxhash_LIB_DIRS_RELWITHDEBINFO
    if(DEFINED ${var_name})                                                             # check if the variable is defined
        set(_${prefix}_LIB_DIRS "${${var_name}}" PARENT_SCOPE)     # e.g., _openssl_LIB_DIRS or _xxhash_LIB_DIRS
    else()
        message(FATAL_ERROR "The variable ${var_name} (${${var_name}}) is not defined.")
    endif()
endfunction()
