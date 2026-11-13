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
#include "libcommonserver/utility/checksumverifier.h"
#include "test_utility/localtemporarydirectory.h"

#include <fstream>

namespace KDC {

namespace {

class LinuxUpdaterMock final : public LinuxUpdater {
    public:
        const VersionInfo &versionInfo() const override { return announcedVersion; }
        void setDownloading() { setState(UpdateState::Downloading); }

        VersionInfo announcedVersion;
        bool relaunchSucceeds{true};
        mutable SyncPath relaunchedPath;
        mutable uint32_t relaunchCount{0};

    private:
        bool launchRelauncher(const SyncPath &targetPath) const override {
            relaunchedPath = targetPath;
            ++relaunchCount;
            return relaunchSucceeds;
        }
};

} // namespace

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

void TestLinuxUpdater::testHasEnoughSpace() {
    constexpr std::streamsize appImageSize = 65 * 1000 * 1000;

    CPPUNIT_ASSERT(LinuxUpdater::hasEnoughSpace(appImageSize, appImageSize));
    CPPUNIT_ASSERT(LinuxUpdater::hasEnoughSpace(appImageSize, 10 * appImageSize));
    CPPUNIT_ASSERT(!LinuxUpdater::hasEnoughSpace(appImageSize, appImageSize - 1));
    CPPUNIT_ASSERT(!LinuxUpdater::hasEnoughSpace(appImageSize, 0));

    // A size the server did not announce does not block the download.
    CPPUNIT_ASSERT(LinuxUpdater::hasEnoughSpace(-1, 0));
}

void TestLinuxUpdater::testInstallUpdateRestarts() {
    const LocalTemporaryDirectory directory("TestLinuxUpdater");
    LinuxUpdaterMock updater;
    updater.announcedVersion.tag = "4.0.0";
    updater._appImagePath = directory.path() / "kDrive.AppImage";
    std::ofstream(updater._appImagePath) << "old AppImage";
    const auto partPath = LinuxUpdater::partialDownloadPath(updater._appImagePath);
    std::ofstream(partPath) << "new AppImage";
    constexpr auto initialPermissions = std::filesystem::perms::owner_read | std::filesystem::perms::group_read;
    std::filesystem::permissions(partPath, initialPermissions);
    updater._downloadInProgress = true;

    uint32_t quitCount = 0;
    updater.setQuitCallback([&] {
        CPPUNIT_ASSERT(std::filesystem::exists(updater._appImagePath));
        CPPUNIT_ASSERT(!std::filesystem::exists(partPath));
        CPPUNIT_ASSERT(std::filesystem::status(updater._appImagePath).permissions() ==
                       (initialPermissions | std::filesystem::perms::owner_exec));
        ++quitCount;
    });

    updater.setDownloading();
    updater.startInstaller();
    CPPUNIT_ASSERT_EQUAL(uint32_t{0}, updater.relaunchCount);
    CPPUNIT_ASSERT_EQUAL(uint32_t{0}, quitCount);

    // The download completion installs the file and requests the restart without another GUI request.
    updater.installUpdate(updater.announcedVersion);
    CPPUNIT_ASSERT_EQUAL(UpdateState::Ready, updater.state());
    CPPUNIT_ASSERT(!updater._downloadInProgress);
    CPPUNIT_ASSERT_EQUAL(updater._appImagePath, updater.relaunchedPath);
    CPPUNIT_ASSERT_EQUAL(uint32_t{1}, quitCount);

    // A second request during shutdown must not launch another instance.
    updater.startInstaller();
    CPPUNIT_ASSERT_EQUAL(uint32_t{1}, updater.relaunchCount);
    CPPUNIT_ASSERT_EQUAL(uint32_t{1}, quitCount);
}

void TestLinuxUpdater::testRecoveredDownloadRestarts() {
    const LocalTemporaryDirectory directory("TestLinuxUpdater");
    LinuxUpdaterMock updater;
    updater.announcedVersion.tag = "4.0.0";
    updater._appImagePath = directory.path() / "kDrive.AppImage";
    const auto partPath = LinuxUpdater::partialDownloadPath(updater._appImagePath);
    std::ofstream(partPath) << "recovered AppImage";
    const auto initialPermissions = std::filesystem::status(partPath).permissions();
    updater._checksumVersion = updater.announcedVersion.fullVersion();
    updater._expectedChecksum = ChecksumVerifier::computeFileChecksum(partPath);
    CPPUNIT_ASSERT(!updater._expectedChecksum.empty());

    bool quitRequested = false;
    updater.setQuitCallback([&] { quitRequested = true; });
    updater.prepareUpdate(updater.announcedVersion);

    CPPUNIT_ASSERT(quitRequested);
    CPPUNIT_ASSERT(std::filesystem::status(updater._appImagePath).permissions() ==
                   (initialPermissions | std::filesystem::perms::owner_exec));
    CPPUNIT_ASSERT_EQUAL(updater._appImagePath, updater.relaunchedPath);
    CPPUNIT_ASSERT(!std::filesystem::exists(partPath));
}

void TestLinuxUpdater::testRelaunchFailureCanBeRetried() {
    const LocalTemporaryDirectory directory("TestLinuxUpdater");
    LinuxUpdaterMock updater;
    updater.announcedVersion.tag = "4.0.0";
    updater._appImagePath = directory.path() / "kDrive.AppImage";
    std::ofstream(LinuxUpdater::partialDownloadPath(updater._appImagePath)) << "new AppImage";
    updater.relaunchSucceeds = false;

    bool quitRequested = false;
    updater.setQuitCallback([&] { quitRequested = true; });
    updater.installUpdate(updater.announcedVersion);
    CPPUNIT_ASSERT_EQUAL(UpdateState::UpdateError, updater.state());
    CPPUNIT_ASSERT(!quitRequested);
    CPPUNIT_ASSERT(std::filesystem::exists(updater._appImagePath));

    updater.relaunchSucceeds = true;
    updater.startInstaller();
    CPPUNIT_ASSERT(quitRequested);
    CPPUNIT_ASSERT_EQUAL(uint32_t{2}, updater.relaunchCount);
}

void TestLinuxUpdater::testOutdatedUpdateDoesNotRestart() {
    const LocalTemporaryDirectory directory("TestLinuxUpdater");
    LinuxUpdaterMock updater;
    updater.announcedVersion.tag = "4.0.0";
    const auto downloadedVersion = updater.announcedVersion;
    updater.announcedVersion.tag = "4.0.1";
    updater._appImagePath = directory.path() / "kDrive.AppImage";
    const auto partPath = LinuxUpdater::partialDownloadPath(updater._appImagePath);
    std::ofstream(partPath) << "outdated AppImage";

    bool quitRequested = false;
    updater.setQuitCallback([&] { quitRequested = true; });
    updater.installUpdate(downloadedVersion);

    CPPUNIT_ASSERT_EQUAL(UpdateState::Available, updater.state());
    CPPUNIT_ASSERT(!quitRequested);
    CPPUNIT_ASSERT_EQUAL(uint32_t{0}, updater.relaunchCount);
    CPPUNIT_ASSERT(!std::filesystem::exists(partPath));
    CPPUNIT_ASSERT(!std::filesystem::exists(updater._appImagePath));
}

} // namespace KDC
