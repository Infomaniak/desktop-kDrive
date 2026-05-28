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

#include "testabstractupdater.h"

#include "db/parmsdb.h"
#include "jobs/syncjobmanager.h"
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
#include "mockversionretriever.h"
#include "mocks/libcommonserver/db/mockdb.h"

namespace KDC {
namespace {
void unskipVersion() {
    createUpdater()->unskipVersion();
}
} // namespace

void TestAbstractUpdater::generateValidVersionInfo(VersionInfo &versionInfo) {
    versionInfo.channel = DistributionChannel::Prod;
    versionInfo.tag = "3.8.2";
    versionInfo.buildVersion = 3;
    versionInfo.downloadUrl = "https://downloads/kDrive-3.8.2.3.exe";
    versionInfo.checksum = "";
    versionInfo.minOsVersion = "";
    versionInfo.minAppVersion = "";
}

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

void TestAbstractUpdater::testCheckUpdateAvailable() {
    auto versionRetriever = std::make_shared<MockVersionRetriever>();
    MockUpdater updater(versionRetriever);

    // Version is up to date
    {
        UniqueId jobId = 0;
        versionRetriever->setUpdateShouldBeAvailable(false);
        (void) updater.checkUpdateAvailable(DistributionChannel::Beta, &jobId);
        while (!SyncJobManagerSingleton::instance()->isJobFinished(jobId)) Utility::msleep(10);
        CPPUNIT_ASSERT(!updater.appShouldBeBlocked());
        CPPUNIT_ASSERT_EQUAL(UpdateState::UpToDate, updater.state());
    }

    // New version available
    {
        UniqueId jobId = 0;
        versionRetriever->setUpdateShouldBeAvailable(true);
        (void) updater.checkUpdateAvailable(DistributionChannel::Beta, &jobId);
        while (!SyncJobManagerSingleton::instance()->isJobFinished(jobId)) Utility::msleep(10);
        CPPUNIT_ASSERT(!updater.appShouldBeBlocked());
        CPPUNIT_ASSERT_EQUAL(UpdateState::Available, updater.state());
    }

    // App is blocked
    {
        UniqueId jobId = 0;
        versionRetriever->setUpdateShouldBeAvailable(true);
        versionRetriever->setBigMinAppVersion(true);
        (void) updater.checkUpdateAvailable(DistributionChannel::Beta, &jobId);
        while (!SyncJobManagerSingleton::instance()->isJobFinished(jobId)) Utility::msleep(10);
        CPPUNIT_ASSERT(updater.appShouldBeBlocked());
        CPPUNIT_ASSERT_EQUAL(UpdateState::Available, updater.state());
        versionRetriever->setBigMinAppVersion(false);
    }

    // OS version is too low
    {
        UniqueId jobId = 0;
        versionRetriever->setUpdateShouldBeAvailable(true);
        versionRetriever->setBigMinOsVersion(true);
        (void) updater.checkUpdateAvailable(DistributionChannel::Beta, &jobId);
        while (!SyncJobManagerSingleton::instance()->isJobFinished(jobId)) Utility::msleep(10);
        CPPUNIT_ASSERT(!updater.appShouldBeBlocked());
        CPPUNIT_ASSERT_EQUAL(UpdateState::UpToDate, updater.state());
        versionRetriever->setBigMinOsVersion(false);
    }
}

} // namespace KDC
