/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#include "testabstractupdater.h"

#include "db/parmsdb.h"
#include "requests/parameterscache.h"

#if defined(KD_MACOS)
#include "server/updater/sparkleupdater.h"
#elif defined(KD_WINDOWS)
#include "server/updater/windowsupdater.h"
#elif defined(KD_LINUX)
#include "server/updater/linuxupdater.h"
#endif

#include "libsyncengine/jobs/jobmanager.h"
#include "version.h"

#include "mockupdater.h"
#include "mockupdatechecker.h"
#include "mocks/libcommonserver/db/mockdb.h"

namespace KDC {

namespace {
void unskipVersion() {
    createUpdater()->unskipVersion();
}
} // namespace

void TestAbstractUpdater::setUp() {
    TestBase::start();
    // Init parmsDb
    bool alreadyExists = false;
    const std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    (void) ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    // Setup parameters cache in test mode
    (void) ParametersCache::instance(true);
}

void TestAbstractUpdater::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    ParametersCache::reset();
    TestBase::stop();
}

void TestAbstractUpdater::testSkipUnskipVersion() {
    const std::string testStr("1.1.1.20210101");
    AbstractUpdater::skipVersion(testStr);

    CPPUNIT_ASSERT_EQUAL(testStr, ParametersCache::instance()->parameters().seenVersion());

    bool found = false;
    Parameters parameters;
    (void) ParmsDb::instance()->selectParameters(parameters, found);
    CPPUNIT_ASSERT(parameters.seenVersion() == testStr);

    unskipVersion();

    CPPUNIT_ASSERT(ParametersCache::instance()->parameters().seenVersion().empty());

    (void) ParmsDb::instance()->selectParameters(parameters, found);
    CPPUNIT_ASSERT(parameters.seenVersion().empty());
}

void TestAbstractUpdater::testIsVersionSkipped() {
    const auto skippedVersion("3.3.3.20210101");

    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped(skippedVersion));

    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("4.3.3.20210101"));
    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("3.5.3.20210101"));
    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("3.3.6.20210101"));
    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("3.3.3.20210109"));

    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("2.3.3.20210101"));
    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("3.1.3.20210101"));
    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("3.3.0.20210101"));
    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("3.3.3.20200101"));

    AbstractUpdater::skipVersion(skippedVersion);

    CPPUNIT_ASSERT(AbstractUpdater::isVersionSkipped(skippedVersion));

    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("4.3.3.20210101"));
    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("3.5.3.20210101"));
    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("3.3.6.20210101"));
    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("3.3.3.20210109"));

    CPPUNIT_ASSERT(AbstractUpdater::isVersionSkipped("2.3.3.20210101"));
    CPPUNIT_ASSERT(AbstractUpdater::isVersionSkipped("3.1.3.20210101"));
    CPPUNIT_ASSERT(AbstractUpdater::isVersionSkipped("3.3.0.20210101"));
    CPPUNIT_ASSERT(AbstractUpdater::isVersionSkipped("3.3.3.20200101"));

    unskipVersion();

    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped(skippedVersion));

    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("4.3.3.20210101"));
    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("3.5.3.20210101"));
    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("3.3.6.20210101"));
    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("3.3.3.20210109"));

    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("2.3.3.20210101"));
    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("3.1.3.20210101"));
    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("3.3.0.20210101"));
    CPPUNIT_ASSERT(!AbstractUpdater::isVersionSkipped("3.3.3.20200101"));
}

void generateValidAllVersionsInfo(AllVersionsInfo &versionsInfo) {
    versionsInfo[VersionChannel::Next].channel = VersionChannel::Next;
    versionsInfo[VersionChannel::Next].tag = "10.0.0";
    versionsInfo[VersionChannel::Next].buildVersion = 20210101;
    versionsInfo[VersionChannel::Next].downloadUrl = "test";

    versionsInfo[VersionChannel::Prod].channel = VersionChannel::Prod;
    versionsInfo[VersionChannel::Prod].tag = "9.0.0";
    versionsInfo[VersionChannel::Prod].buildVersion = 20210101;
    versionsInfo[VersionChannel::Prod].downloadUrl = "test";

    versionsInfo[VersionChannel::Beta].channel = VersionChannel::Beta;
    versionsInfo[VersionChannel::Beta].tag = "11.0.0";
    versionsInfo[VersionChannel::Beta].buildVersion = 20210101;
    versionsInfo[VersionChannel::Beta].downloadUrl = "test";

    versionsInfo[VersionChannel::Internal].channel = VersionChannel::Internal;
    versionsInfo[VersionChannel::Internal].tag = "11.0.1";
    versionsInfo[VersionChannel::Internal].buildVersion = 20210101;
    versionsInfo[VersionChannel::Internal].downloadUrl = "test";
}

void TestAbstractUpdater::testCurrentVersionedChannel() {
    const auto updateChecker = std::make_shared<MockUpdateChecker>();
    MockUpdater updater(updateChecker);

    AllVersionsInfo testVersions;
    generateValidAllVersionsInfo(testVersions);
    updateChecker->setAllVersionInfo(testVersions);

    std::string version;
    updater.setMockGetCurrentVersion([&version]() { return version; });

    // Check Next version
    version = "10.0.0.20210101";
    CPPUNIT_ASSERT_EQUAL(VersionChannel::Next, updater.currentVersionChannel());

    // Check Prod version
    version = "9.0.0.20210101";
    CPPUNIT_ASSERT_EQUAL(VersionChannel::Prod, updater.currentVersionChannel());

    // Check Beta version
    version = "11.0.0.20210101";
    CPPUNIT_ASSERT_EQUAL(VersionChannel::Beta, updater.currentVersionChannel());

    // Check Internal version
    version = "11.0.1.20210101";
    CPPUNIT_ASSERT_EQUAL(VersionChannel::Internal, updater.currentVersionChannel());

    // Check Legacy version
    version = "8.0.0.20210101";
    CPPUNIT_ASSERT_EQUAL(VersionChannel::Legacy, updater.currentVersionChannel());

    // Check Unknown version (higher than prod)
    version = "9.0.0.20210102";
    CPPUNIT_ASSERT_EQUAL(VersionChannel::Unknown, updater.currentVersionChannel());

    // Emtpy version info.
    updateChecker->setAllVersionInfo({});
    version = "11.0.1.20210101";
    CPPUNIT_ASSERT_EQUAL(VersionChannel::Unknown, updater.currentVersionChannel());

    // Non-empty invalid version info.
    const AllVersionsInfo invalidVersions;
    testVersions[VersionChannel::Unknown].tag = "10.0.0";
    updateChecker->setAllVersionInfo(invalidVersions);
    version = "11.0.1.20210101";
    CPPUNIT_ASSERT_EQUAL(VersionChannel::Unknown, updater.currentVersionChannel());
}

void TestAbstractUpdater::testOsTooOld() {
    // New version is available
    {
        const auto updateChecker = std::make_shared<MockUpdateChecker>();
        updateChecker->setVersionReceived(true);

        AllVersionsInfo testVersions;
        generateValidAllVersionsInfo(testVersions);
        updateChecker->setAllVersionInfo(testVersions);

        MockUpdater updater(updateChecker);
        updater.onAppVersionReceived();
        CPPUNIT_ASSERT_EQUAL(UpdateState::Available, updater.state());
    }
    // New version is available but OS is too old
    {
        const auto updateChecker = std::make_shared<MockUpdateChecker>();
        updateChecker->setVersionReceived(true);

        AllVersionsInfo testVersions;
        generateValidAllVersionsInfo(testVersions);
        testVersions[VersionChannel::Next].buildMinOsVersion = "99.99.99";
        testVersions[VersionChannel::Prod].buildMinOsVersion = "99.99.99";
        testVersions[VersionChannel::Beta].buildMinOsVersion = "99.99.99";
        testVersions[VersionChannel::Internal].buildMinOsVersion = "99.99.99";
        updateChecker->setAllVersionInfo(testVersions);

        MockUpdater updater(updateChecker);
        updater.onAppVersionReceived();
        CPPUNIT_ASSERT_EQUAL(UpdateState::UpToDate, updater.state());
    }
}

} // namespace KDC
