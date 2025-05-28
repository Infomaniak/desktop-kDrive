#! /bin/bash
#
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

# ----------------------------------------------------------------------
# This script is used to find the dependencies of a package in the conan cache.
PACKAGE="$1"
VERSION="$2"
reference="${PACKAGE}/${VERSION}"

if [[ -z "$PACKAGE" ]] || [[ -z "$VERSION" ]]; then
  echo "Usage: $0 <package> <version>" >&2
  exit 1
fi

if ! command -v sqlite3 &> /dev/null; then
  echo "Please install sqlite3 to use this script." >&2
  exit 1
fi

conan_home=$(conan config home)
conan_db="$conan_home/p/cache.sqlite3"
if [[ ! -f "$conan_db" ]]; then
  echo "Conan database not found at $conan_db. Please execute this script from the root of the project: ./infomaniak-build-tools/conan/build_dependencies.sh" >&2
  exit 1
fi

# Here is the schema of the conan table 'packages':
# CREATE TABLE packages (reference text NOT NULL, rrev text NOT NULL, pkgid text, prev text, path text NOT NULL UNIQUE, timestamp real NOT NULL, build_id text, lru integer NOT NULL , UNIQUE(reference, rrev, pkgid, prev));

# Here is an example of a line in the table for the reference 'xxhash/0.8.2':
# reference    | rrev                             | pkgid                                    | prev                             | path                 | timestamp        | build_id | lru
# xxhash/0.8.2   36767978e4497f669787e6ddcc2d38c8   e1f17ad175d69603dc832665bb47fb4725b03d98   39af6a512db2b67d724b15d4ec2c7d7c   b/xxhas74a0fa3c20650   1747649225.53109   <NULL>      1748000951
sql_req="SELECT path FROM packages WHERE reference='$reference' ORDER BY timestamp DESC LIMIT 1; .exit"

subpath=$(sqlite3 "$conan_db" "$sql_req" 2>/dev/null)
if [[ -z "$subpath" ]]; then
  echo "Package $reference not found in conan cache." >&2
  exit 1
fi

printf "%s\n" "$conan_home/p/$subpath/p"