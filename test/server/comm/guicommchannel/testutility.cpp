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

#include "comm/guijobs/utilityactivateloadinfojob.h"
#include "comm/guijobs/utilitybestvfsavailablemodejob.h"
#include "comm/guijobs/utilityfindgoodpathfornewsyncjob.h"
#include "comm/guijobs/utilityispathvalidfornewsyncjob.h"
#include "comm/guijobs/utilitycheckcommstatusjob.h"
#include "comm/guijobs/utilityhassystemlaunchonstartupjob.h"
#include "comm/guijobs/utilitygetappstatejob.h"
#include "comm/guijobs/utilitysetappstatejob.h"
#include "comm/guijobs/utilitysendlogtosupportjob.h"
#include "comm/guijobs/utilitycancellogtosupportjob.h"
#include "comm/guijobs/utilitygetlogestimatedsizejob.h"
#include "comm/guijobs/utilityquitjob.h"
#include "comm/guijobs/utilitydisplayclientreportjob.h"

#include "testguicommchannel.h"
#include "../testcommhelpers.h"

namespace KDC {

using namespace testcommhelpers;

void TestGuiCommChannel::testUtilityActivateLoadInfoJob() {
    const Poco::JSON::Object query = createSimpleQuery(RequestNum::UTILITY_ACTIVATELOADINFO);
    const auto queryStr = stringifyQueryObj(query);

    // Job expected answers
    const SimpleAnswers simpleAnswers = createSimpleAnswers(RequestNum::UTILITY_ACTIVATELOADINFO);
    const auto answerStr = stringifyAnswerObj(simpleAnswers.answerWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        const auto activateLoadInfoJob = std::dynamic_pointer_cast<UtilityActivateLoadInfoJob>(job);
        CPPUNIT_ASSERT(activateLoadInfoJob);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(simpleAnswers.answer);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUtilityBestVfsAvailableModeJob() {
    // Query
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::UTILITY_BESTVFSAVAILABLEMODE));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("path", "/dummy");
    (void) queryParamsObj.set("driveDbId", 0);

    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("bestMode", toInt(VirtualFileMode::Off));
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::UTILITY_BESTVFSAVAILABLEMODE));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto bestVfsAvailableModeJob = std::dynamic_pointer_cast<UtilityBestVfsAvailableModeJob>(job);
        CPPUNIT_ASSERT(bestVfsAvailableModeJob);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUtilityFindGoodPathForNewSyncJob() {
    // Query
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::UTILITY_FINDGOODPATHFORNEWSYNC));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("basePath", "");
    (void) queryParamsObj.set("driveDbId", 0);

    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("errorMessage", "");
    (void) paramsObj.set("goodPath", toBase64("/dummy/good_path"));
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::UTILITY_FINDGOODPATHFORNEWSYNC));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto findGoodPathForNewSyncJob = std::dynamic_pointer_cast<UtilityFindGoodPathForNewSyncJob>(job);
        CPPUNIT_ASSERT(findGoodPathForNewSyncJob);
        findGoodPathForNewSyncJob->_goodPath = SyncPath{"/dummy/good_path"};
        findGoodPathForNewSyncJob->_errorMessage = "";
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUtilityIsPathValidForNewSyncJob() {
    // Query
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::UTILITY_ISPATHVALIDFORNEWSYNC));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("path", "");
    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("isValid", true);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::UTILITY_ISPATHVALIDFORNEWSYNC));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto isPathValidForNewSyncJob = std::dynamic_pointer_cast<UtilityIsPathValidForNewSyncJob>(job);
        CPPUNIT_ASSERT(isPathValidForNewSyncJob);
        isPathValidForNewSyncJob->_isValid = true;
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUtilityCheckCommStatusJob() {
    const Poco::JSON::Object query = createSimpleQuery(RequestNum::UTILITY_CHECKCOMMSTATUS);
    const auto queryStr = stringifyQueryObj(query);

    // Job expected answers
    const SimpleAnswers simpleAnswers = createSimpleAnswers(RequestNum::UTILITY_CHECKCOMMSTATUS);
    const auto answerStr = stringifyAnswerObj(simpleAnswers.answerWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        const auto utilityCheckCommStatusJob = std::dynamic_pointer_cast<UtilityCheckCommStatusJob>(job);
        CPPUNIT_ASSERT(utilityCheckCommStatusJob);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(simpleAnswers.answer);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUtilityHasSystemLaunchOnStartupJob() {
    const auto query = createSimpleQuery(RequestNum::UTILITY_HASSYSTEMLAUNCHONSTARTUP);
    const auto queryStr = stringifyQueryObj(query);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("enabled", true);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::UTILITY_HASSYSTEMLAUNCHONSTARTUP));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto utilityHasSystemLaunchOnStartupJob = std::dynamic_pointer_cast<UtilityHasSystemLaunchOnStartupJob>(job);
        CPPUNIT_ASSERT(utilityHasSystemLaunchOnStartupJob);

        utilityHasSystemLaunchOnStartupJob->_enabled = true;
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUtilitySetAppStateJob() {
    // Query
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::UTILITY_SET_APPSTATE));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("key", 0);
    (void) queryParamsObj.set("value", 123);
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
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::UTILITY_SET_APPSTATE));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto setAppStateJob = std::dynamic_pointer_cast<UtilitySetAppStateJob>(job);
        CPPUNIT_ASSERT(setAppStateJob);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUtilityGetAppStateJob() {
    // Query
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::UTILITY_GET_APPSTATE));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("key", 0);
    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("value", 123);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::UTILITY_GET_APPSTATE));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto getAppStateJob = std::dynamic_pointer_cast<UtilityGetAppStateJob>(job);
        getAppStateJob->_value = 123;
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUtilitySendLogToSupportJob() {
    // Query
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::UTILITY_SEND_LOG_TO_SUPPORT));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("includeArchivedLogs", 1);
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
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::UTILITY_SEND_LOG_TO_SUPPORT));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto utilitySendLogToSupportJob = std::dynamic_pointer_cast<UtilitySendLogToSupportJob>(job);
        CPPUNIT_ASSERT(utilitySendLogToSupportJob);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUtilityCancelLogToSupportJob() {
    const Poco::JSON::Object query = createSimpleQuery(RequestNum::UTILITY_CANCEL_LOG_TO_SUPPORT);
    const auto queryStr = stringifyQueryObj(query);

    // Job expected answers
    const SimpleAnswers simpleAnswers = createSimpleAnswers(RequestNum::UTILITY_CANCEL_LOG_TO_SUPPORT);
    const auto answerStr = stringifyAnswerObj(simpleAnswers.answerWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto utilityCancelLogToSupportJob = std::dynamic_pointer_cast<UtilityCancelLogToSupportJob>(job);
        CPPUNIT_ASSERT(utilityCancelLogToSupportJob);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(simpleAnswers.answer);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUtilityGetLogEstimatedSizeJob() {
    // Query
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::UTILITY_GET_LOG_ESTIMATED_SIZE));

    Poco::JSON::Object queryParamsObj;
    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("logSize", 99999);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::UTILITY_GET_LOG_ESTIMATED_SIZE));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto utilityGetLogEstimatedSizeJob = std::dynamic_pointer_cast<UtilityGetLogEstimatedSizeJob>(job);
        utilityGetLogEstimatedSizeJob->_logSize = 99999;
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUtilityQuitJob() {
    const Poco::JSON::Object query = createSimpleQuery(RequestNum::UTILITY_QUIT);
    const auto queryStr = stringifyQueryObj(query);

    // Job expected answers
    const SimpleAnswers simpleAnswers = createSimpleAnswers(RequestNum::UTILITY_QUIT);
    const auto answerStr = stringifyAnswerObj(simpleAnswers.answerWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        const auto utilityQuitJob = std::dynamic_pointer_cast<UtilityQuitJob>(job);
        CPPUNIT_ASSERT(utilityQuitJob);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(simpleAnswers.answer);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUtilityDisplayClientReportJob() {
    const Poco::JSON::Object query = createSimpleQuery(RequestNum::UTILITY_SEND_APP_START_TRACE);
    const auto queryStr = stringifyQueryObj(query);

    // Job expected answers
    const SimpleAnswers simpleAnswers = createSimpleAnswers(RequestNum::UTILITY_SEND_APP_START_TRACE);
    const auto answerStr = stringifyAnswerObj(simpleAnswers.answerWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        const auto utilityDisplayClientReportJob = std::dynamic_pointer_cast<UtilityDisplayClientReportJob>(job);
        CPPUNIT_ASSERT(utilityDisplayClientReportJob);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(simpleAnswers.answer);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

} // namespace KDC
