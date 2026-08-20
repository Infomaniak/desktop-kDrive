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

#include "db/parmsdb.h"
#include "requests/parameterscache.h"
#include "io/iohelper.h"
#include "jobs/network/kDrive_API/downloadjob.h"
#include "jobs/network/directdownloadjob.h"
#include "jobs/network/infomaniak_API/getappversionjob.h"
#include "keychainmanager/keychainmanager.h"
#include "mocks/mockkeychainstorage.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "test_utility/localtemporarydirectory.h"
#include "test_utility/testhelpers.h"
#include "updater/windowsupdater.h"
#include "updater/checksumverifier.h"
#include "utility/digitalsignaturechecker_win.h"
#include "jobs/syncjobmanager.h"

#include <Poco/SHA2Engine.h>
#include <Poco/DigestEngine.h>

#include <array>
#include <fstream>

namespace KDC {

void TestWindowsUpdater::setUp() {
    TestBase::start();

    testhelpers::setupLogging();
    const testhelpers::TestVariables testVariables;

    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    const std::string keychainKey("123");
    (void) KeyChainManager::instance(std::make_shared<MockKeyChainStorage>());
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
    // A ChecksumVerifier that always produces a matching checksum by computing the actual file's SHA-256.
    class MatchingChecksumVerifier final : public ChecksumVerifier {
        public:
            void setInstallerPath(const SyncPath &installerPath) { _installerPath = installerPath; }

        private:
            bool downloadSha256File([[maybe_unused]] const std::string &sha256Url, std::string &outChecksum) override {
                std::ifstream file(_installerPath, std::ios::binary);
                if (!file) return false;
                Poco::SHA2Engine sha256(Poco::SHA2Engine::ALGORITHM::SHA_256);
                std::array<char, 8192> buffer{};
                while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
                    sha256.update(buffer.data(), static_cast<std::size_t>(file.gcount()));
                }
                outChecksum = Poco::DigestEngine::digestToHex(sha256.digest());
                return !outChecksum.empty();
            }

            SyncPath _installerPath;
    };

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

            bool verifyDigitalSignature([[maybe_unused]] const SyncPath &filepath) override { return true; }

            SyncPath _installerPath;
    };

    const LocalTemporaryDirectory tempDir("TestWindowsUpdater");
    WindowsUpdaterMock testObj;
    auto checksumVerifier = std::make_shared<MatchingChecksumVerifier>();
    testObj._checksumVerifier = checksumVerifier;
    const auto dummyInstallerPath = tempDir.path() / "TestWindowsUpdater";
    testObj.setInstallerPath(dummyInstallerPath);
    checksumVerifier->setInstallerPath(dummyInstallerPath);

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
    CPPUNIT_ASSERT(!DigitalSignatureChecker_win("").isSignatureValid());
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

void TestWindowsUpdater::testIsSignatureValidExtended() {
    if (!testhelpers::isExtendedTest()) return;

    static const std::string appUid("1234567890");
    static const std::vector<DistributionChannel> channels = {DistributionChannel::Internal, DistributionChannel::Beta,
                                                              DistributionChannel::Prod};

    User user;
    bool found = false;
    (void) ParmsDb::instance()->selectUser(1, user, found);
    CPPUNIT_ASSERT(found);
    const std::vector<UserId> userIdList = {user.userId()};

    const LocalTemporaryDirectory tmpDir("TestWindowsUpdater");

    // Fetch the download link of each channel, keeping only distinct URLs to avoid downloading the same version twice.
    std::map<std::string, DistributionChannel, std::less<>> downloadUrls;
    for (const auto channel: channels) {
        GetAppVersionJob job(channel, appUid, userIdList);
        (void) job.runSynchronously();
        CPPUNIT_ASSERT(!job.hasHttpError());

        const auto &versionInfo = job.versionInfo();
        CPPUNIT_ASSERT(versionInfo.isValid());
        CPPUNIT_ASSERT(!versionInfo.downloadUrl.empty());

        (void) downloadUrls.try_emplace(versionInfo.downloadUrl, channel);
    }

    // Download each distinct version and check its digital signature.
    int8_t index = 0;
    for (const auto &[downloadUrl, channel]: downloadUrls) {
        const auto installerPath = tmpDir.path() / ("installer-" + std::to_string(index++) + ".exe");

        DirectDownloadJob downloadJob(installerPath, downloadUrl);
        (void) downloadJob.runSynchronously();
        CPPUNIT_ASSERT(!downloadJob.hasHttpError());
        CPPUNIT_ASSERT(std::filesystem::exists(installerPath));

        CPPUNIT_ASSERT_MESSAGE(
                "Digital signature is invalid for installer: " + toString(channel) + " - " + installerPath.string(),
                DigitalSignatureChecker_win(SyncPath(installerPath)).isSignatureValid());
    }
}

void TestWindowsUpdater::testIsChecksumValid() {
    // A mock that lets tests inject the sha256-sidecar download result.
    class ChecksumVerifierMock final : public ChecksumVerifier {
        public:
            enum class Sha256Result { Success, Failure, Empty };

            void setSha256Result(const Sha256Result result) { _sha256Result = result; }
            void setExpectedChecksum(const std::string &checksum) { _expectedChecksum = checksum; }

        private:
            bool downloadSha256File([[maybe_unused]] const std::string &sha256Url, std::string &outChecksum) override {
                if (_sha256Result == Sha256Result::Failure) return false;
                outChecksum = (_sha256Result == Sha256Result::Empty) ? std::string{} : _expectedChecksum;
                return true;
            }

            Sha256Result _sha256Result{Sha256Result::Success};
            std::string _expectedChecksum;
    };

    static const std::string validChecksumValue("3d735840895bcb958f359009b06cbe9b840ae9e2df22651f431bfec4ac7b696f");
    static const std::string invalidChecksumValue("083a301369cd711e9803f7d90d342a3778f9cb864ab22992b49fccddc3b9256c");

    const LocalTemporaryDirectory tmpDir("TestWindowsUpdater");
    IoError ioError = IoError::Success;
    (void) IoHelper::copyFileOrDirectory(testhelpers::localTestDirPath() / "test_pictures/picture-1.jpg", tmpDir.path(), ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    const SyncPath installerPath = tmpDir.path() / "picture-1.jpg";

    // Case 1: sha256 sidecar download fails -> update must be blocked.
    {
        ChecksumVerifierMock verifier;
        verifier.setSha256Result(ChecksumVerifierMock::Sha256Result::Failure);
        CPPUNIT_ASSERT(!verifier.verifyFileChecksum(installerPath, "https://downloads/kDrive-3.8.2.3.exe"));
    }

    // Case 2: sha256 sidecar downloaded but is empty -> update must be blocked.
    {
        // Re-create the file since the previous case deleted it on failure.
        (void) IoHelper::copyFileOrDirectory(testhelpers::localTestDirPath() / "test_pictures/picture-1.jpg", tmpDir.path(),
                                             ioError);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        ChecksumVerifierMock verifier;
        verifier.setSha256Result(ChecksumVerifierMock::Sha256Result::Empty);
        CPPUNIT_ASSERT(!verifier.verifyFileChecksum(installerPath, "https://downloads/kDrive-3.8.2.3.exe"));
    }

    // Case 3: sha256 sidecar downloaded, checksum matches -> verification passes.
    // The file must still exist after the call.
    {
        // Re-create the file since previous cases deleted it.
        (void) IoHelper::copyFileOrDirectory(testhelpers::localTestDirPath() / "test_pictures/picture-1.jpg", tmpDir.path(),
                                             ioError);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        ChecksumVerifierMock verifier;
        verifier.setSha256Result(ChecksumVerifierMock::Sha256Result::Success);
        verifier.setExpectedChecksum(validChecksumValue);
        CPPUNIT_ASSERT(verifier.verifyFileChecksum(installerPath, "https://downloads/kDrive-3.8.2.3.exe"));
        CPPUNIT_ASSERT(std::filesystem::exists(installerPath));
    }

    // Case 4: sha256 sidecar downloaded, checksum mismatch -> verification fails, file deleted.
    {
        // Re-create the file since case 3 passed (file was not deleted).
        (void) IoHelper::copyFileOrDirectory(testhelpers::localTestDirPath() / "test_pictures/picture-1.jpg", tmpDir.path(),
                                             ioError);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        ChecksumVerifierMock verifier;
        verifier.setSha256Result(ChecksumVerifierMock::Sha256Result::Success);
        verifier.setExpectedChecksum(invalidChecksumValue);
        CPPUNIT_ASSERT(!verifier.verifyFileChecksum(installerPath, "https://downloads/kDrive-3.8.2.3.exe"));
        CPPUNIT_ASSERT(!std::filesystem::exists(installerPath)); // File must have been deleted.
    }
}

} // namespace KDC
