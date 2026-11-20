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
#include "db/parmsdb.h"
#include "libcommonserver/utility/checksumverifier.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "requests/parameterscache.h"
#include "test_utility/localtemporarydirectory.h"
#include "version.h"

#include <fstream>

namespace KDC {

namespace {

class LinuxUpdaterMock final : public LinuxUpdater {
    public:
        const VersionInfo &versionInfo() const override { return announcedVersion; }
        void setDownloading() { setState(UpdateState::Downloading); }
        void setAvailable() { setState(UpdateState::Available); }

        VersionInfo announcedVersion;
        bool relaunchSucceeds{true};
        mutable SyncPath relaunchedPath;
        mutable bool relaunchedOnSettings{false};
        mutable uint32_t relaunchCount{0};

    private:
        bool launchRelauncher(const SyncPath &targetPath, const RelaunchWindow relaunchWindow) const override {
            relaunchedPath = targetPath;
            relaunchedOnSettings = relaunchWindow == RelaunchWindow::Settings;
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
    // The user started the preparation from Settings.
    updater._downloadInProgress = true;
    updater._restartWhenInstalled = true;

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

    // The download completion installs the file and restarts on the Settings window without another GUI request.
    updater.installUpdate(updater.announcedVersion);
    CPPUNIT_ASSERT_EQUAL(UpdateState::Ready, updater.state());
    CPPUNIT_ASSERT(!updater._downloadInProgress);
    CPPUNIT_ASSERT_EQUAL(updater._appImagePath, updater.relaunchedPath);
    CPPUNIT_ASSERT(updater.relaunchedOnSettings);
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
    updater._restartWhenInstalled = true;
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
    updater._restartWhenInstalled = true;

    bool quitRequested = false;
    updater.setQuitCallback([&] { quitRequested = true; });
    updater.installUpdate(updater.announcedVersion);
    CPPUNIT_ASSERT_EQUAL(UpdateState::UpdateError, updater.state());
    CPPUNIT_ASSERT(!quitRequested);
    CPPUNIT_ASSERT(std::filesystem::exists(updater._appImagePath));

    // The retry is the restart the client requests on the update already installed.
    updater.relaunchSucceeds = true;
    updater.startInstaller();
    CPPUNIT_ASSERT(quitRequested);
    CPPUNIT_ASSERT_EQUAL(uint32_t{2}, updater.relaunchCount);
    CPPUNIT_ASSERT(!updater.relaunchedOnSettings);
}

void TestLinuxUpdater::testAutomaticInstallDefersRestart() {
    const LocalTemporaryDirectory directory("TestLinuxUpdater");
    LinuxUpdaterMock updater;
    updater.announcedVersion.tag = "4.0.0";
    updater._appImagePath = directory.path() / "kDrive.AppImage";
    std::ofstream(LinuxUpdater::partialDownloadPath(updater._appImagePath)) << "new AppImage";

    uint32_t quitCount = 0;
    updater.setQuitCallback([&] { ++quitCount; });

    // An automatic preparation installs the file without interrupting the user.
    updater.installUpdate(updater.announcedVersion);
    CPPUNIT_ASSERT_EQUAL(UpdateState::Ready, updater.state());
    CPPUNIT_ASSERT(std::filesystem::exists(updater._appImagePath));
    CPPUNIT_ASSERT_EQUAL(uint32_t{0}, updater.relaunchCount);
    CPPUNIT_ASSERT_EQUAL(uint32_t{0}, quitCount);

    // A later check finding the same version keeps waiting for the restart request.
    updater.onUpdateFound();
    CPPUNIT_ASSERT_EQUAL(UpdateState::Ready, updater.state());
    CPPUNIT_ASSERT_EQUAL(uint32_t{0}, updater.relaunchCount);

    // The client's request restarts in the background, without opening Settings.
    updater.startInstaller();
    CPPUNIT_ASSERT_EQUAL(uint32_t{1}, updater.relaunchCount);
    CPPUNIT_ASSERT(!updater.relaunchedOnSettings);
    CPPUNIT_ASSERT_EQUAL(uint32_t{1}, quitCount);
}

void TestLinuxUpdater::testChannelChangeDoesNotInstall() {
    const LocalTemporaryDirectory directory("TestLinuxUpdater");
    bool alreadyExists = false;
    (void) ParmsDb::instance(MockDb::makeDbName(alreadyExists), KDRIVE_VERSION_STRING, true, true);
    (void) ParametersCache::instance(true);
    ParametersCache::instance()->parameters().setAutoUpdates(true);
    const auto previousAppImagePath = CommonUtility::appImagePath();
    const auto appImagePath = directory.path() / "kDrive.AppImage";
    CommonUtility::setAppImagePath(appImagePath);

    LinuxUpdaterMock updater;
    updater.announcedVersion.tag = "4.0.0";
    updater._appImagePath = appImagePath;
    updater.setAvailable();

    // Changing the channel is a setting: the update stays available for the user to start it.
    updater.onUpdateFoundAfterChannelChange();
    CPPUNIT_ASSERT_EQUAL(UpdateState::Available, updater.state());
    CPPUNIT_ASSERT(!updater._downloadInProgress);
    CPPUNIT_ASSERT_EQUAL(uint32_t{0}, updater.relaunchCount);

    CommonUtility::setAppImagePath(previousAppImagePath);
    ParmsDb::instance()->close();
    ParmsDb::reset();
    ParametersCache::reset();
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
