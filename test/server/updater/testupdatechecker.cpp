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

#include "jobs/jobmanager.h"
#include "jobs/network/getappversionjob.h"
#include "libcommon/utility/utility.h"
#include "requests/parameterscache.h"
#include "mockupdatechecker.h"
#include "utility/utility.h"

namespace KDC {

void TestUpdateChecker::setUp() {
    ParametersCache::instance(true);
}

void TestUpdateChecker::testCheckUpdateAvailable() {
    // Version is higher than current version
    {
        MockUpdateChecker testObj;
        UniqueId jobId = 0;
        testObj.setUpdateShoudBeAvailable(true);
        testObj.checkUpdateAvailability(&jobId);
        while (!JobManager::instance()->isJobFinished(jobId)) Utility::msleep(10);
        CPPUNIT_ASSERT(testObj.versionInfo(DistributionChannel::Beta).isValid());
    }

    // Version is lower than current version
    {
        MockUpdateChecker testObj;
        UniqueId jobId = 0;
        testObj.setUpdateShoudBeAvailable(false);
        testObj.checkUpdateAvailability(&jobId);
        while (!JobManager::instance()->isJobFinished(jobId)) Utility::msleep(10);
        CPPUNIT_ASSERT(testObj.versionInfo(DistributionChannel::Beta).isValid());
    }
}

VersionInfo getVersionInfo(const DistributionChannel channel, const VersionValue versionNumber) {
    VersionInfo versionInfo;
    versionInfo.channel = channel;
    versionInfo.tag = tag(versionNumber);
    versionInfo.buildVersion = buildVersion(versionNumber);
    versionInfo.downloadUrl = "test";
    return versionInfo;
}

void TestUpdateChecker::testVersionInfo() {
    UpdateChecker testObj;
    testObj._prodVersionChannel = DistributionChannel::Prod;

    auto testFunc = [&testObj](const VersionValue expectedValue, const DistributionChannel expectedChannel,
                               const DistributionChannel selectedChannel, const std::vector<VersionValue> &versionsNumber,
                               const CPPUNIT_NS::SourceLine &sourceline) {
        testObj._versionsInfo.clear();
        testObj._versionsInfo.try_emplace(DistributionChannel::Prod,
                                          getVersionInfo(DistributionChannel::Prod, versionsNumber[0]));
        testObj._versionsInfo.try_emplace(DistributionChannel::Beta,
                                          getVersionInfo(DistributionChannel::Beta, versionsNumber[1]));
        testObj._versionsInfo.try_emplace(DistributionChannel::Internal,
                                          getVersionInfo(DistributionChannel::Internal, versionsNumber[2]));
        const auto &versionInfo = testObj.versionInfo(selectedChannel);
        CPPUNIT_NS::assertEquals(expectedChannel, versionInfo.channel, sourceline, "");
        CPPUNIT_NS::assertEquals(tag(expectedValue), versionInfo.tag, sourceline, "");
        CPPUNIT_NS::assertEquals(buildVersion(expectedValue), versionInfo.buildVersion, sourceline, "");
    };

    // selected version: Prod
    /// versions values: Prod > Beta > Internal
    testFunc(High, DistributionChannel::Prod, DistributionChannel::Prod, {High, Medium, Low}, CPPUNIT_SOURCELINE());
    /// versions values: Internal > Beta > Prod
    testFunc(Low, DistributionChannel::Prod, DistributionChannel::Prod, {Low, Medium, High}, CPPUNIT_SOURCELINE());
    /// versions values: Prod == Beta == Internal
    testFunc(Medium, DistributionChannel::Prod, DistributionChannel::Prod, {Medium, Medium, Medium}, CPPUNIT_SOURCELINE());

    // selected version: Beta
    /// versions values: Prod > Beta > Internal
    testFunc(High, DistributionChannel::Prod, DistributionChannel::Beta, {High, Medium, Low}, CPPUNIT_SOURCELINE());
    /// versions values: Internal > Beta > Prod
    testFunc(Medium, DistributionChannel::Beta, DistributionChannel::Beta, {Low, Medium, High}, CPPUNIT_SOURCELINE());
    /// versions values: Prod == Beta == Internal
    testFunc(Medium, DistributionChannel::Prod, DistributionChannel::Beta, {Medium, Medium, Medium}, CPPUNIT_SOURCELINE());

    // selected version: Internal
    /// versions values: Prod > Beta > Internal
    testFunc(High, DistributionChannel::Prod, DistributionChannel::Internal, {High, Medium, Low}, CPPUNIT_SOURCELINE());
    /// versions values: Internal > Beta > Prod
    testFunc(High, DistributionChannel::Internal, DistributionChannel::Internal, {Low, Medium, High}, CPPUNIT_SOURCELINE());
    /// versions values: Beta > Prod > Internal
    testFunc(High, DistributionChannel::Beta, DistributionChannel::Internal, {Medium, High, Low}, CPPUNIT_SOURCELINE());
    /// versions values: Prod > Internal > Beta
    testFunc(High, DistributionChannel::Prod, DistributionChannel::Internal, {High, Low, Medium}, CPPUNIT_SOURCELINE());
    /// versions values: Beta > Internal > Prod
    testFunc(High, DistributionChannel::Beta, DistributionChannel::Internal, {Low, High, Medium}, CPPUNIT_SOURCELINE());
    /// versions values: Prod == Beta == Internal
    testFunc(Medium, DistributionChannel::Prod, DistributionChannel::Internal, {Medium, Medium, Medium}, CPPUNIT_SOURCELINE());
    /// versions values:  Beta == Prod > Internal
    testFunc(High, DistributionChannel::Prod, DistributionChannel::Internal, {High, High, Low}, CPPUNIT_SOURCELINE());
    /// versions values: Beta == Internal > Prod
    testFunc(Medium, DistributionChannel::Beta, DistributionChannel::Internal, {Low, Medium, Medium}, CPPUNIT_SOURCELINE());
    /// versions values: Prod == Internal > Beta
    testFunc(Medium, DistributionChannel::Prod, DistributionChannel::Internal, {Medium, Low, Medium}, CPPUNIT_SOURCELINE());
}

} // namespace KDC
