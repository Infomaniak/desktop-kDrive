#!/usr/bin/env bash

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

set -ex
SRCDIR="${1-$PWD}"
BUILDDIR="$PWD/build-linux"
CLIENTDIR="$BUILDDIR/client"
INSTALLDIR="$BUILDDIR/install"

rm -Rf "${BUILDDIR}"
mkdir -p "${BUILDDIR}"
mkdir -p "${CLIENTDIR}"
mkdir -p "${INSTALLDIR}"

podman machine stop build_kdrive
ulimit -n unlimited
podman machine start build_kdrive
podman run --rm -it \
	--privileged \
	--ulimit nofile=4000000:4000000 \
	--volume /mnt/src:/src \
	--volume /mnt/build:/build \
	--volume /mnt/install:/install \
	--workdir "/src" \
	--env APPLICATION_SERVER_URL="$APPLICATION_SERVER_URL" \
	--env KDRIVE_VERSION_BUILD="$(date +%Y%m%d)" \
	--arch arm64 \
	ghcr.io/infomaniak/kdrive-desktop-linux:latest /bin/bash -c "/src/infomaniak-build-tools/linux/appimage-build-arm64.sh"
podman machine stop build_kdrive

VERSION=$(grep "KDRIVE_VERSION_FULL" "$BUILDDIR/client/version.h" | awk '{print $3}')

SUFFIX="master"

echo $SUFFIX
mv "${INSTALLDIR}/kDrive-${SUFFIX}-arm64.AppImage" $INSTALLDIR/kDrive-$VERSION-arm64.AppImage

rm -Rf "${BUILDDIR}-arm64"
mv "${BUILDDIR}" "${BUILDDIR}-arm64"
