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

#include "testwindowsupdater.h"

#include "testabstractupdater.h"
#include "db/parmsdb.h"
#include "requests/parameterscache.h"
#include "io/iohelper.h"
#include "jobs/network/kDrive_API/downloadjob.h"
#include "keychainmanager/keychainmanager.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "test_utility/localtemporarydirectory.h"
#include "test_utility/testhelpers.h"
#include "updater/windowsupdater.h"
#include "utility/digitalsignaturechecker_win.h"
#include "mockupdatechecker.h"
#include "jobs/syncjobmanager.h"

namespace KDC {

void TestWindowsUpdater::setUp() {
    TestBase::start();

    testhelpers::setupLogging();
    const testhelpers::TestVariables testVariables;

    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    const std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());
    // Create parmsDb
    bool alreadyExists = false;
    (void) ParmsDb::instance(MockDb::makeDbName(alreadyExists), KDRIVE_VERSION_STRING, true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    // Insert user, account & drive
    const int userId(atoi(testVariables.userId.c_str()));
    const User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    const int accountId(atoi(testVariables.accountId.c_str()));
    const Account account(1, accountId, user.dbId(), "account1");
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    const int driveId = atoi(testVariables.driveId.c_str());
    const Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    // Setup parameters cache in test mode
    (void) ParametersCache::instance(true);
}

void TestWindowsUpdater::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    ParametersCache::reset();
    SyncJobManagerSingleton::instance()->stop();
    SyncJobManagerSingleton::clear();
    TestBase::stop();
}

void TestWindowsUpdater::testOnUpdateFound() {
    class WindowsUpdaterMock final : public WindowsUpdater {
        public:
            void setInstallerPath(const SyncPath &installerPath) { _installerPath = installerPath; }

        private:
            [[nodiscard]] bool getInstallerPath(SyncPath &path) const override {
                path = _installerPath;
                return true;
            }

            void downloadUpdate() noexcept override { setState(UpdateState::Downloading); }

            std::streamsize getExpectedInstallerSize([[maybe_unused]] const std::string &downloadUrl) override { return 10; }

            bool verifyDigitalSignature(const SyncPath &filepath) override { return true; }

            SyncPath _installerPath;
    };

    const LocalTemporaryDirectory tempDir("TestWindowsUpdater");
    WindowsUpdaterMock testObj;
    const auto dummyInstallerPath = tempDir.path() / "TestWindowsUpdater";
    testObj.setInstallerPath(dummyInstallerPath);

    // Installer is not yet downloaded.
    testObj.onUpdateFound();
    CPPUNIT_ASSERT_EQUAL(UpdateState::Downloading, testObj.state());

    // Installer exists locally but is empty.
    testhelpers::generateTestFile(dummyInstallerPath);

    testObj.onUpdateFound();
    CPPUNIT_ASSERT_EQUAL(UpdateState::Downloading, testObj.state());
    CPPUNIT_ASSERT(std::filesystem::exists(dummyInstallerPath)); // Local file has been removed.

    // Installer exists locally but is incomplete.
    testhelpers::generateOrEditTestFile(dummyInstallerPath);

    testObj.onUpdateFound();
    CPPUNIT_ASSERT_EQUAL(UpdateState::Downloading, testObj.state());
    CPPUNIT_ASSERT(std::filesystem::exists(dummyInstallerPath)); // Local file has been removed.

    // Installer exists locally and is valid.
    testhelpers::generateTestFile(dummyInstallerPath, 10);

    testObj.onUpdateFound();
    CPPUNIT_ASSERT_EQUAL(UpdateState::Ready, testObj.state());
    CPPUNIT_ASSERT(std::filesystem::exists(dummyInstallerPath));
}

void TestWindowsUpdater::testIsSignatureValid() {
    // Empty path.
    CPPUNIT_ASSERT(!DigitalSignatureChecker_win({}).isSignatureValid());
    // Path to non-existing file.
    CPPUNIT_ASSERT(!DigitalSignatureChecker_win(SyncPath("A/B/C")).isSignatureValid());
    // Path to existing file but not signed.
    const LocalTemporaryDirectory tmpDir;
    const auto testPath = tmpDir.path() / "testSignature.txt";
    testhelpers::generateOrEditTestFile(testPath);
    CPPUNIT_ASSERT(!DigitalSignatureChecker_win(SyncPath(testPath)).isSignatureValid());
    // Path to an existing signed file.
    {
        const auto cacheDirectory = std::make_shared<CacheDirectory>(tmpDir.path());
        const testhelpers::TestVariables testVariables;
        static const NodeId signedFileId = "5304421";
        const auto signedFilePath = tmpDir.path() / "testfile.exe";
        DownloadJob job(nullptr, cacheDirectory, DownloadJob::FileDownloadInfo{_driveDbId, signedFileId, signedFilePath, 0},
                        DownloadJob::DateTimePolicy::ApplyDateTime);
        (void) job.runSynchronously();
        CPPUNIT_ASSERT(DigitalSignatureChecker_win(SyncPath(signedFilePath)).isSignatureValid());
    }
}

void TestWindowsUpdater::testIsChecksumValid() {
    struct TestCase {
            std::string checksumValue;
            std::string fileName;
            bool expectedValid;
    };

    static const std::string noChecksumValue("");
    static const std::string invalidChecksumValue("083a301369cd711e9803f7d90d342a3778f9cb864ab22992b49fccddc3b9256c");
    static const std::string validChecksumValue("3d735840895bcb958f359009b06cbe9b840ae9e2df22651f431bfec4ac7b696f");

    const std::vector<TestCase> testCases = {
            {noChecksumValue, "picture-1.jpg", true}, // kstore is missing checksum
            {invalidChecksumValue, "picture-1111.jpg", false}, // can't calculate checksum (file doesn't exist)
            {invalidChecksumValue, "picture-1.jpg", false}, // checksum is invalid
            {validChecksumValue, "picture-1.jpg", true}, // checksum is valid
    };

    for (const auto &testCase: testCases) {
        LocalTemporaryDirectory tmpDir;
        IoError ioError = IoError::Success;

        // Only copy file if it's expected to exist
        if (testCase.fileName == "picture-1.jpg") {
            (void) IoHelper::copyFileOrDirectory(testhelpers::localTestDirPath() / "test_pictures/picture-1.jpg", tmpDir.path(),
                                                 ioError);
            CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        }

        WindowsUpdater updater;

        AllVersionsInfo versionInfo;
        TestAbstractUpdater::generateValidAllVersionsInfo(versionInfo);
        auto testUpdateChecker = std::make_shared<MockUpdateChecker>();

        testUpdateChecker->setAllVersionInfo(versionInfo);
        testUpdateChecker->setVersionReceived(true);
        testUpdateChecker->setChecksumForAllChannels(testCase.checksumValue);
        updater._updateChecker = testUpdateChecker;

        CPPUNIT_ASSERT_EQUAL(testCase.expectedValid, updater.verifyFileChecksum(tmpDir.path() / testCase.fileName));
    }
}

} // namespace KDC
