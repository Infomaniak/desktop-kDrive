/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "testlinuxupdater.h"

#include "server/updater/linuxupdater.h"

namespace KDC {

void TestLinuxUpdater::testInstallTargetPath() {
    const SyncPath directory = "/home/user/Applications";

    // Published file names are normalized, whatever the version and architecture.
    CPPUNIT_ASSERT_EQUAL(directory / "kDrive.AppImage",
                         LinuxUpdater::installTargetPath(directory / "kDrive-4.0.0.99-amd64.AppImage"));
    CPPUNIT_ASSERT_EQUAL(directory / "kDrive.AppImage",
                         LinuxUpdater::installTargetPath(directory / "kDrive-4.1.12.3-arm64.AppImage"));

    // The stable name is replaced in place.
    CPPUNIT_ASSERT_EQUAL(directory / "kDrive.AppImage", LinuxUpdater::installTargetPath(directory / "kDrive.AppImage"));

    // Names chosen by the user, or by an AppImage manager, are kept.
    for (const auto *name: {"kdrive", "kDrive-latest.AppImage", "kDrive-4.0.0.99-amd64.AppImage.old", "kDrive_4.0.0.99.AppImage",
                            "kdrive-4.0.0.99-amd64.AppImage", "kDriveRecoveryUpdater-4.0.0.99-amd64.AppImage"}) {
        CPPUNIT_ASSERT_EQUAL(directory / name, LinuxUpdater::installTargetPath(directory / name));
    }
}

} // namespace KDC
