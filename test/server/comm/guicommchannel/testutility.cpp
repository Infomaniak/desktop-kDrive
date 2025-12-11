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
#include "comm/guijobs/utilitycheckcommstatusjob.h"
#include "comm/guijobs/utilityhassystemlaunchonstartupjob.h"
#include "comm/guijobs/utilityquitjob.h"
#include "comm/guijobs/utilitydisplayclientreportjob.h"

#include "testguicommchannel.h"
#include "../testcommhelpers.h"

namespace KDC {

using namespace testcommhelpers;

void TestGuiCommChannel::testUtilityActivateLoadInfoJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::UTILITY_ACTIVATELOADINFO)) +
                        R"(,)"
                        R"( "params": { } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::UTILITY_ACTIVATELOADINFO)) +
                        R"(,)"
                        R"( "params": { } })"};

    // Callback expected answer
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{}})"};
#endif

    // Job expected answer
    const auto answerStr{R"({ "cause": 0,)"
                         R"( "code": 0,)"
                         R"( "id": 1,)"
                         R"( "num": )" +
                         std::to_string(toInt(RequestNum::UTILITY_ACTIVATELOADINFO)) +
                         R"(,)"
                         R"( "params": {  }, "type": )" +
                         std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto activateLoadInfoJob = std::dynamic_pointer_cast<UtilityActivateLoadInfoJob>(job);
        CPPUNIT_ASSERT(activateLoadInfoJob);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
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
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
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
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
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
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(simpleAnswers.answer);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUtilityDisplayClientReportJob() {
    const Poco::JSON::Object query = createSimpleQuery(RequestNum::UTILITY_DISPLAY_CLIENT_REPORT);
    const auto queryStr = stringifyQueryObj(query);

    // Job expected answers
    const SimpleAnswers simpleAnswers = createSimpleAnswers(RequestNum::UTILITY_DISPLAY_CLIENT_REPORT);
    const auto answerStr = stringifyAnswerObj(simpleAnswers.answerWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        const auto utilityDisplayClientReportJob = std::dynamic_pointer_cast<UtilityDisplayClientReportJob>(job);
        CPPUNIT_ASSERT(utilityDisplayClientReportJob);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(simpleAnswers.answer);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

} // namespace KDC
