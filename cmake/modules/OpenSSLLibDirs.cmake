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
find_package(OpenSSL 3.1.0 REQUIRED SSL Crypto)
string(TOUPPER "${CMAKE_BUILD_TYPE}" _BUILD_TYPE_UPPER)

if(APPLE)
    set(_OpenSSL_LIB_DIRS "${openssl-universal_LIB_DIRS_${_BUILD_TYPE_UPPER}}")
elseif(UNIX)
    set(_OpenSSL_LIB_DIRS "${openssl_LIB_DIRS_${_BUILD_TYPE_UPPER}}")
endif()
