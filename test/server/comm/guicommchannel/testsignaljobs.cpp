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

#include "comm/guijobs/signalutilityshownotificationjob.h"
#include "comm/guijobs/signalutilityshowsettingsjob.h"
#include "comm/guijobs/signalutilityshowsynthesisjob.h"
#include "comm/guijobs/signalutilityloguploadstatejob.h"
#include "comm/guijobs/signalutilityquitjob.h"

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

void TestGuiCommChannel::testSignalUtilityShowNotificationJob() {
    const CommString title = Str("Notification title");
    const CommString message = Str("Message: the item 'C:/Users/kDrive/item/côté_au_carré.pdf' has been updated");
    SignalUtilityShowNotificationJob job(title, message);
    checkSignalCommonMethods(job, SignalNum::UTILITY_SHOW_NOTIFICATION);
    CPPUNIT_ASSERT_EQUAL(title, Str(job._title));
    CPPUNIT_ASSERT_EQUAL(message, job._message);
}

void TestGuiCommChannel::testSignalUtilityShowSettingsJob() {
    SignalUtilityShowSettingsJob job;
    checkSignalCommonMethods(job, SignalNum::UTILITY_SHOW_SETTINGS);
}

void TestGuiCommChannel::testSignalUtilityShowSynthesisJob() {
    SignalUtilityShowSynthesisJob job;
    checkSignalCommonMethods(job, SignalNum::UTILITY_SHOW_SYNTHESIS);
}

void TestGuiCommChannel::testSignalUtilityLogUploadStateJob() {
    SignalUtilityLogUploadStateJob job(LogUploadState::Success, 100);
    checkSignalCommonMethods(job, SignalNum::UTILITY_LOG_UPLOAD_STATUS_UPDATED);
    CPPUNIT_ASSERT_EQUAL(LogUploadState::Success, job._state);
    CPPUNIT_ASSERT_EQUAL(int32_t{100}, job._percentage);
}

void TestGuiCommChannel::testSignalUtilityQuitJob() {
    SignalUtilityQuitJob job;
    checkSignalCommonMethods(job, SignalNum::UTILITY_QUIT);
}

} // namespace KDC
