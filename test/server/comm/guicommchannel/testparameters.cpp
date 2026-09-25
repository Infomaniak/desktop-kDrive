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

#include "comm/guijobs/parametersinfojob.h"
#include "comm/guijobs/parametersupdatejob.h"

#include "testguicommchannel.h"
#include "../testcommhelpers.h"

namespace KDC {

using namespace testcommhelpers;

namespace {
ServerParameters getExpectedParameters() {
    const ProxyConfig proxyConfig(ProxyType::HTTP, "myHostName", 6666, true, "john.doe", "1234");

    ServerParameters parameters;
    parameters.setLanguage(Language::Default);
    parameters.setMonoIcons(false);
    parameters.setAutoStart(true);
    parameters.setMoveToTrash(true);
    parameters.setNotificationsDisabled(NotificationsDisabled::Never);
    parameters.setUseLog(true);
    parameters.setLogLevel(LogLevel::Debug);
    parameters.setExtendedLog(true);
    parameters.setPurgeOldLogs(true);
#ifdef KD_MACOS // darkTheme only on macOS
    parameters.setDarkTheme(true);
#else
    parameters.setDarkTheme(false);
#endif
    parameters.setDialogGeometry("preferencesWindow", "blob1234");
    parameters.setDialogGeometry("drivePreferencesPanel", "blob4567");
    parameters.setMaxAllowedCpu(50);
    parameters.setProxyConfig(proxyConfig);
    return parameters;
};

Poco::JSON::Object createParametersObject() {
    Poco::JSON::Object parametersObj;
    (void) parametersObj.set("language", toInt(Language::Default));
    (void) parametersObj.set("monoIcons", false);
    (void) parametersObj.set("autoStart", true);
    (void) parametersObj.set("moveToTrash", true);
    (void) parametersObj.set("notificationsDisabled", toInt(NotificationsDisabled::Never));
    (void) parametersObj.set("useLog", true);
    (void) parametersObj.set("logLevel", toInt(LogLevel::Debug));
    (void) parametersObj.set("extendedLog", true);
    (void) parametersObj.set("purgeOldLogs", true);

    Poco::JSON::Object proxyConfigObj;
    (void) proxyConfigObj.set("type", toInt(ProxyType::HTTP));
    (void) proxyConfigObj.set("hostName", toBase64(Str("myHostName")));
    (void) proxyConfigObj.set("port", 6666);
    (void) proxyConfigObj.set("needsAuth", true);
    (void) proxyConfigObj.set("user", toBase64(Str("john.doe")));
    (void) proxyConfigObj.set("pwd", toBase64(Str("1234")));

    (void) parametersObj.set("proxyConfigInfo", proxyConfigObj);
#ifdef KD_MACOS
    (void) parametersObj.set("darkTheme", true);
#endif

    Poco::JSON::Object dialogGeometryObj;
    (void) dialogGeometryObj.set("preferencesWindow", toBase64(Str("blob1234")));
    (void) dialogGeometryObj.set("drivePreferencesPanel", toBase64(Str("blob4567")));

    (void) parametersObj.set("dialogGeometry", dialogGeometryObj);
    (void) parametersObj.set("maxAllowedCpu", 50);
    (void) parametersObj.set("distributionChannel", toInt(DistributionChannel::Prod));
    (void) parametersObj.set("sentryEnabled", true);
    (void) parametersObj.set("matomoEnabled", true);

    return parametersObj;
};
} // namespace

void TestGuiCommChannel::testParametersJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::PARAMETERS_INFO));
    const Poco::JSON::Object queryParamsObj;
    (void) queryObj.set("params", queryParamsObj);

    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("parametersInfo", createParametersObject());
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::PARAMETERS_INFO));
    (void) answerObjWithNumAndType.set("type", toInt(GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto parametersInfoJob = std::dynamic_pointer_cast<ParametersInfoJob>(job);
        CPPUNIT_ASSERT(parametersInfoJob);
        parametersInfoJob->_parameters = getExpectedParameters();
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
    (void) queryParamsObj.set("parametersInfo", createParametersObject());
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
    (void) answerObjWithNumAndType.set("type", toInt(GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto parametersUpdateJob = std::dynamic_pointer_cast<ParametersUpdateJob>(job);
        CPPUNIT_ASSERT(parametersUpdateJob);
        const Parameters res = getExpectedParameters();
        CPPUNIT_ASSERT(res == parametersUpdateJob->_parameters);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

} // namespace KDC
