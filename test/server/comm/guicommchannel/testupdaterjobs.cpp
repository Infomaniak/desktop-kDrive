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

#include "testguicommchannel.h"
#include "../testcommhelpers.h"

#include "comm/guijobs/updaterversioninfojob.h"
#include "comm/guijobs/updaterstatejob.h"
#include "comm/guijobs/updaterstartinstallerjob.h"
#include "comm/guijobs/updaterskipversionjob.h"

namespace KDC {

using namespace testcommhelpers;


void TestGuiCommChannel::testUpdaterVersionInfoJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::UPDATER_VERSION_INFO));
    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("channel", toInt(VersionChannel::Prod));
    (void) queryObj.set("params", queryParamsObj);

    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object versionInfoObj;
    (void) versionInfoObj.set("channel", toInt(VersionChannel::Prod));
    (void) versionInfoObj.set("tag", toBase64(Str("3.8.2")));
    (void) versionInfoObj.set("buildVersion", 3);
    (void) versionInfoObj.set("buildMinOsVersion", toBase64(Str("10.15")));
    (void) versionInfoObj.set("downloadUrl", toBase64(Str("https://downloads/kDrive-3.8.2.3.pkg")));

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("versionInfo", versionInfoObj);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::UPDATER_VERSION_INFO));
    (void) answerObjWithNumAndType.set("type", toInt(GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto updaterVersionInfoJob = std::dynamic_pointer_cast<UpdaterVersionInfoJob>(job);
        CPPUNIT_ASSERT(updaterVersionInfoJob);

        VersionInfo versionInfo;
        versionInfo.channel = VersionChannel::Prod;
        versionInfo.tag = "3.8.2";
        versionInfo.buildVersion = 3;
        versionInfo.buildMinOsVersion = "10.15";
        versionInfo.downloadUrl = "https://downloads/kDrive-3.8.2.3.pkg";

        updaterVersionInfoJob->_versionInfo = versionInfo;
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUpdaterStateJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::UPDATER_STATE));
    Poco::JSON::Object queryParamsObj;
    (void) queryObj.set("params", queryParamsObj);

    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("updateState", toInt(UpdateState::Checking));
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::UPDATER_STATE));
    (void) answerObjWithNumAndType.set("type", toInt(GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto updaterStateJob = std::dynamic_pointer_cast<UpdaterStateJob>(job);
        CPPUNIT_ASSERT(updaterStateJob);

        updaterStateJob->_updateState = UpdateState::Checking;
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUpdaterStartInstallerJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::UPDATER_START_INSTALLER));
    Poco::JSON::Object queryParamsObj;
    (void) queryObj.set("params", queryParamsObj);

    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::UPDATER_START_INSTALLER));
    (void) answerObjWithNumAndType.set("type", toInt(GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto updaterStartInstallerJob = std::dynamic_pointer_cast<UpdaterStartInstallerJob>(job);
        CPPUNIT_ASSERT(updaterStartInstallerJob);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUpdaterSkipVersionJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::UPDATER_SKIP_VERSION));
    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("skippedVersion", toBase64(Str("3.8.2 (build 1)")));
    (void) queryObj.set("params", queryParamsObj);

    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::UPDATER_SKIP_VERSION));
    (void) answerObjWithNumAndType.set("type", toInt(GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto updaterSkipVersionJob = std::dynamic_pointer_cast<UpdaterSkipVersionJob>(job);
        CPPUNIT_ASSERT(updaterSkipVersionJob);
        CPPUNIT_ASSERT(CommonUtility::str2CommString("3.8.2 (build 1)") == updaterSkipVersionJob->_skippedVersion);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

} // namespace KDC
