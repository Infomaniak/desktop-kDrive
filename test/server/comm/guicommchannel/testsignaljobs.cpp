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


#include "comm/guijobs/signalaccountupdatedjob.h"
#include "comm/guijobs/signaldriveupdatedjob.h"

#include "comm/guijobs/signalupdatershowdialogjob.h"

#include "testguicommchannel.h"
#include "../testcommhelpers.h"

namespace KDC {

using namespace testcommhelpers;

void TestGuiCommChannel::checkSignalCommonMethods(AbstractGuiJob &guiJob, const SignalNum signalNum) {
    CPPUNIT_ASSERT_EQUAL(signalNum, guiJob.signalNum());
    CPPUNIT_ASSERT(guiJob.deserializeInputParms());
    CPPUNIT_ASSERT(guiJob.process());
    CPPUNIT_ASSERT(guiJob.serializeOutputParms());
}

void TestGuiCommChannel::testSignalAccountUpdatedJob() {
    AccountInfo accountInfo(1, 666);
    accountInfo.setAccountId(1001);
    SignalAccountUpdatedJob job(accountInfo);
    checkSignalCommonMethods(job, SignalNum::ACCOUNT_UPDATED);
    CPPUNIT_ASSERT(accountInfo == job._accountInfo);
}

void TestGuiCommChannel::testSignalDriveUpdatedJob() {
    DriveInfo driveInfo;
    driveInfo.setDbId(1);
    driveInfo.setId(2);
    driveInfo.setAccountDbId(3);
    driveInfo.setAdmin(true);
    driveInfo.setAccessDenied(true);
    driveInfo.setMaintenance(true);
    driveInfo.setSize(1000000000);
    driveInfo.setUsedSize(50000000);
    SignalDriveUpdatedJob job(driveInfo);

    checkSignalCommonMethods(job, SignalNum::DRIVE_UPDATED);
    CPPUNIT_ASSERT(driveInfo == job._driveInfo);
}

void TestGuiCommChannel::testSignaUpdaterShowDialogJob() {
    VersionInfo versionInfo;
    versionInfo.channel = VersionChannel::Beta;
    versionInfo.tag = "4.0.0";
    versionInfo.buildVersion = 1;
    versionInfo.buildMinOsVersion = "15.1";
    versionInfo.downloadUrl = "https://downloads/kDrive/latest";

    SignalUpdaterShowDialogJob job(versionInfo);

    checkSignalCommonMethods(job, SignalNum::UPDATER_SHOW_DIALOG);
    CPPUNIT_ASSERT(versionInfo == job._versionInfo);
}

} // namespace KDC
