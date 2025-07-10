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

#include "testupdatechecker.h"

#include "jobs/syncjobmanager.h"
#include "jobs/network/getappversionjob.h"
#include "libcommon/utility/utility.h"
#include "requests/parameterscache.h"
#include "mockupdatechecker.h"
#include "utility/utility.h"

namespace KDC {

void TestUpdateChecker::setUp() {
    TestBase::start();
    ParametersCache::instance(true);
}

void TestUpdateChecker::tearDown() {
    ParametersCache::reset();
    SyncJobManager::instance()->stop();
    SyncJobManager::clear();
    TestBase::stop();
}

void TestUpdateChecker::testCheckUpdateAvailable() {
    // Version is higher than current version
    {
        MockUpdateChecker testObj;
        UniqueId jobId = 0;
        testObj.setUpdateShoudBeAvailable(true);
        testObj.checkUpdateAvailability(&jobId);
        while (!SyncJobManager::instance()->isJobFinished(jobId)) Utility::msleep(10);
        CPPUNIT_ASSERT(testObj.versionInfo(VersionChannel::Beta).isValid());
    }

    // Version is lower than current version
    {
        MockUpdateChecker testObj;
        UniqueId jobId = 0;
        testObj.setUpdateShoudBeAvailable(false);
        testObj.checkUpdateAvailability(&jobId);
        while (!SyncJobManager::instance()->isJobFinished(jobId)) Utility::msleep(10);
        CPPUNIT_ASSERT(testObj.versionInfo(VersionChannel::Beta).isValid());
    }
}

VersionInfo getVersionInfo(const VersionChannel channel, const VersionValue versionNumber) {
    VersionInfo versionInfo;
    versionInfo.channel = channel;
    versionInfo.tag = tag(versionNumber);
    versionInfo.buildVersion = buildVersion(versionNumber);
    versionInfo.downloadUrl = "test";
    return versionInfo;
}

void TestUpdateChecker::testVersionInfo() {
    UpdateChecker testObj;
    testObj._prodVersionChannel = VersionChannel::Prod;

    // Check the returned value when versionInfo is not available.
    testObj._isVersionReceived = false;
    CPPUNIT_ASSERT(testObj._defaultVersionInfo == testObj.versionInfo(VersionChannel::Prod));
    CPPUNIT_ASSERT(testObj._defaultVersionInfo == testObj.versionInfo(VersionChannel::Next));
    CPPUNIT_ASSERT(testObj._defaultVersionInfo == testObj.versionInfo(VersionChannel::Beta));
    CPPUNIT_ASSERT(testObj._defaultVersionInfo == testObj.versionInfo(VersionChannel::Internal));

    auto testFunc = [&testObj](const VersionValue expectedValue, const VersionChannel expectedChannel,
                               const VersionChannel selectedChannel, const std::vector<VersionValue> &versionsNumber,
                               const CPPUNIT_NS::SourceLine &sourceline) {
        testObj._versionsInfo.clear();
        testObj._isVersionReceived = true;
        testObj._versionsInfo.try_emplace(VersionChannel::Prod, getVersionInfo(VersionChannel::Prod, versionsNumber[0]));
        testObj._versionsInfo.try_emplace(VersionChannel::Beta, getVersionInfo(VersionChannel::Beta, versionsNumber[1]));
        testObj._versionsInfo.try_emplace(VersionChannel::Internal, getVersionInfo(VersionChannel::Internal, versionsNumber[2]));
        const auto &versionInfo = testObj.versionInfo(selectedChannel);
        CPPUNIT_NS::assertEquals(expectedChannel, versionInfo.channel, sourceline, "");
        CPPUNIT_NS::assertEquals(tag(expectedValue), versionInfo.tag, sourceline, "");
        CPPUNIT_NS::assertEquals(buildVersion(expectedValue), versionInfo.buildVersion, sourceline, "");
    };

    // selected version: Prod
    /// versions values: Prod > Beta > Internal
    testFunc(High, VersionChannel::Prod, VersionChannel::Prod, {High, Medium, Low}, CPPUNIT_SOURCELINE());
    /// versions values: Internal > Beta > Prod
    testFunc(Low, VersionChannel::Prod, VersionChannel::Prod, {Low, Medium, High}, CPPUNIT_SOURCELINE());
    /// versions values: Prod == Beta == Internal
    testFunc(Medium, VersionChannel::Prod, VersionChannel::Prod, {Medium, Medium, Medium}, CPPUNIT_SOURCELINE());

    // selected version: Beta
    /// versions values: Prod > Beta > Internal
    testFunc(High, VersionChannel::Prod, VersionChannel::Beta, {High, Medium, Low}, CPPUNIT_SOURCELINE());
    /// versions values: Internal > Beta > Prod
    testFunc(Medium, VersionChannel::Beta, VersionChannel::Beta, {Low, Medium, High}, CPPUNIT_SOURCELINE());
    /// versions values: Prod == Beta == Internal
    testFunc(Medium, VersionChannel::Prod, VersionChannel::Beta, {Medium, Medium, Medium}, CPPUNIT_SOURCELINE());

    // selected version: Internal
    /// versions values: Prod > Beta > Internal
    testFunc(High, VersionChannel::Prod, VersionChannel::Internal, {High, Medium, Low}, CPPUNIT_SOURCELINE());
    /// versions values: Internal > Beta > Prod
    testFunc(High, VersionChannel::Internal, VersionChannel::Internal, {Low, Medium, High}, CPPUNIT_SOURCELINE());
    /// versions values: Beta > Prod > Internal
    testFunc(High, VersionChannel::Beta, VersionChannel::Internal, {Medium, High, Low}, CPPUNIT_SOURCELINE());
    /// versions values: Prod > Internal > Beta
    testFunc(High, VersionChannel::Prod, VersionChannel::Internal, {High, Low, Medium}, CPPUNIT_SOURCELINE());
    /// versions values: Beta > Internal > Prod
    testFunc(High, VersionChannel::Beta, VersionChannel::Internal, {Low, High, Medium}, CPPUNIT_SOURCELINE());
    /// versions values: Prod == Beta == Internal
    testFunc(Medium, VersionChannel::Prod, VersionChannel::Internal, {Medium, Medium, Medium}, CPPUNIT_SOURCELINE());
    /// versions values:  Beta == Prod > Internal
    testFunc(High, VersionChannel::Prod, VersionChannel::Internal, {High, High, Low}, CPPUNIT_SOURCELINE());
    /// versions values: Beta == Internal > Prod
    testFunc(Medium, VersionChannel::Beta, VersionChannel::Internal, {Low, Medium, Medium}, CPPUNIT_SOURCELINE());
    /// versions values: Prod == Internal > Beta
    testFunc(Medium, VersionChannel::Prod, VersionChannel::Internal, {Medium, Low, Medium}, CPPUNIT_SOURCELINE());
}

} // namespace KDC
