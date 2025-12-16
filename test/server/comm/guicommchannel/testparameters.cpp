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

#include "comm/guijobs/parametersinfojob.h"
#include "comm/guijobs/parametersupdatejob.h"

#include "testguicommchannel.h"
#include "../testcommhelpers.h"

namespace KDC {

using namespace testcommhelpers;

namespace {
ParametersInfo getExpectedParametersInfo() {
    const ProxyConfigInfo proxyConfigInfo(ProxyType::HTTP, "myHostName", 6666, true, "john.doe", "1234");
    const ParametersInfo::DialogGeometry dialogGeometry = {{"preferencesWindow", "blob1234"},
                                                           {"drivePreferencesPanel", "blob4567"}};

    ParametersInfo parametersInfo(Language::Default, false, true, true, NotificationsDisabled::Never, true, LogLevel::Debug, true,
                                  true,
#ifdef KD_MACOS // darkTheme only on macOS
                                  true
#else
                                  false
#endif
                                  ,
                                  false, dialogGeometry, 50);

    parametersInfo.setProxyConfigInfo(proxyConfigInfo);

    return parametersInfo;
};

Poco::JSON::Object createParametersInfoObject() {
    Poco::JSON::Object parametersInfoObj;
    (void) parametersInfoObj.set("language", toInt(Language::Default));
    (void) parametersInfoObj.set("monoIcons", false);
    (void) parametersInfoObj.set("autoStart", true);
    (void) parametersInfoObj.set("moveToTrash", true);
    (void) parametersInfoObj.set("notificationsDisabled", toInt(NotificationsDisabled::Never));
    (void) parametersInfoObj.set("useLog", true);
    (void) parametersInfoObj.set("logLevel", toInt(LogLevel::Debug));
    (void) parametersInfoObj.set("extendedLog", true);
    (void) parametersInfoObj.set("purgeOldLogs", true);

    Poco::JSON::Object proxyConfigInfoObj;
    (void) proxyConfigInfoObj.set("type", toInt(ProxyType::HTTP));
    (void) proxyConfigInfoObj.set("hostName", toBase64(Str("myHostName")));
    (void) proxyConfigInfoObj.set("port", 6666);
    (void) proxyConfigInfoObj.set("needsAuth", true);
    (void) proxyConfigInfoObj.set("user", toBase64(Str("john.doe")));
    (void) proxyConfigInfoObj.set("pwd", toBase64(Str("1234")));

    (void) parametersInfoObj.set("proxyConfigInfo", proxyConfigInfoObj);
#ifdef KD_MACOS
    (void) parametersInfoObj.set("darkTheme", true);
#endif
    (void) parametersInfoObj.set("showShortcuts", false);

    Poco::JSON::Object dialogGeometryObj;
    (void) dialogGeometryObj.set("preferencesWindow", toBase64(Str("blob1234")));
    (void) dialogGeometryObj.set("drivePreferencesPanel", toBase64(Str("blob4567")));

    (void) parametersInfoObj.set("dialogGeometry", dialogGeometryObj);
    (void) parametersInfoObj.set("maxAllowedCpu", 50);
    (void) parametersInfoObj.set("distributionChannel", toInt(VersionChannel::Prod));
    (void) parametersInfoObj.set("sentryEnabled", false);
    (void) parametersInfoObj.set("matomoEnabled", false);

    return parametersInfoObj;
};
} // namespace

void TestGuiCommChannel::testParametersInfoJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::PARAMETERS_INFO));
    (void) queryObj.set("params", {});

    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("parametersInfo", createParametersInfoObject());
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::PARAMETERS_INFO));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto parametersInfoJob = std::dynamic_pointer_cast<ParametersInfoJob>(job);
        CPPUNIT_ASSERT(parametersInfoJob);
        parametersInfoJob->_parametersInfo = getExpectedParametersInfo();
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testParametersUpdateJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::PARAMETERS_UPDATE));
    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("parametersInfo", createParametersInfoObject());
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
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::PARAMETERS_UPDATE));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto parametersUpdateJob = std::dynamic_pointer_cast<ParametersUpdateJob>(job);
        CPPUNIT_ASSERT(parametersUpdateJob);
        ParametersInfo res = getExpectedParametersInfo();
        CPPUNIT_ASSERT(res == parametersUpdateJob->_parametersInfo);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

} // namespace KDC
