#!/bin/bash

#
# Infomaniak kDrive - Desktop
# Copyright (C) 2023-2024 Infomaniak Network SA
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
#

./svgtoicns Error.svg
mv Error.icns ../icns/error.icns

./svgtoicns Error_Shared.svg
mv Error_Shared.icns ../icns/error_swm.icns

./svgtoicns OK.svg
mv OK.icns ../icns/ok.icns

./svgtoicns Sync.svg
mv Sync.icns ../icns/sync.icns

./svgtoicns Sync_Shared.svg
mv Sync_Shared.icns ../icns/sync_swm.icns

./svgtoicns Warning.svg
mv Warning.icns ../icns/warning.icns

./svgtoicns Warning_Shared.svg
mv Warning_Shared.icns ../icns/warning_swm.icns
