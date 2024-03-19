#!/bin/sh

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

# Dimensions taken from https://www.apriorit.com/dev-blog/357-shell-extentions-basics-samples-common-problems#_Toc408244375
convert -background transparent attention.svg -gravity SouthWest \( -clone 0 -resize 10x10 -extent 16x16 \) \( -clone 0 -resize 16x16 -extent 32x32 \) \( -clone 0 -resize 24x24 -extent 48x48 \) \( -clone 0 -resize 128x128 -extent 256x256 \) -delete 0 ../../../windows/KDOverlays/ico/Warning.ico
convert -background transparent error.svg -gravity SouthWest \( -clone 0 -resize 10x10 -extent 16x16 \) \( -clone 0 -resize 16x16 -extent 32x32 \) \( -clone 0 -resize 24x24 -extent 48x48 \) \( -clone 0 -resize 128x128 -extent 256x256 \) -delete 0 ../../../windows/KDOverlays/ico/Error.ico
convert -background transparent ok.svg -gravity SouthWest \( -clone 0 -resize 10x10 -extent 16x16 \) \( -clone 0 -resize 16x16 -extent 32x32 \) \( -clone 0 -resize 24x24 -extent 48x48 \) \( -clone 0 -resize 128x128 -extent 256x256 \) -delete 0 ../../../windows/KDOverlays/ico/OK.ico
convert -background transparent sync.svg -gravity SouthWest \( -clone 0 -resize 10x10 -extent 16x16 \) \( -clone 0 -resize 16x16 -extent 32x32 \) \( -clone 0 -resize 24x24 -extent 48x48 \) \( -clone 0 -resize 128x128 -extent 256x256 \) -delete 0 ../../../windows/KDOverlays/ico/Sync.ico
