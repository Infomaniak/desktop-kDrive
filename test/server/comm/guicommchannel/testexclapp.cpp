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

#if defined(KD_MACOS)

#include "testguicommchannel.h"
#include "comm/guijobs/exclappgetlistjob.h"
#include "comm/guijobs/exclappsetlistjob.h"
#include "comm/guijobs/exclappgetfetchingapplistjob.h"
#include "libcommon/comm.h"
#include "log/log.h"

namespace KDC {

namespace {
std::string toBase64(const std::string &input) {
    std::string output;
    CommonUtility::convertToBase64Str(input, output);
    return output;
}

CommString beautifulString(const Poco::JSON::Object &obj) {
    std::ostringstream oss;
    Poco::JSON::Stringifier::stringify(obj, oss, 1, 0);

    const CommString _answerStr = oss.str();
    Poco::JSON::Parser parser;
    Poco::Dynamic::Var dynamicVar = parser.parse(CommonUtility::commString2Str(_answerStr));
    Poco::DynamicStruct paramsStruct = *dynamicVar.extract<Poco::JSON::Object::Ptr>();

    return Poco::Dynamic::structToString(paramsStruct);
}

CommString stringifyQueryObj(const Poco::JSON::Object &obj) {
    return beautifulString(obj);
}

CommString stringifyAnswerObj(const Poco::JSON::Object &obj) {
    return beautifulString(obj);
}

CommString stringifyCbkAnswerObj(const Poco::JSON::Object &obj) {
    std::ostringstream json;
    Poco::JSON::Stringifier::stringify(obj, json, 0); // compact form

    return json.str();
}

} // namespace

void TestGuiCommChannel::testExclAppGetListJob() {
    // Query. No need to pass a request id as the response is via a callback.
    Poco::JSON::Object queryObj;
    (void) queryObj.set("num", toInt(RequestNum::EXCLAPP_GETLIST));
    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("default", false);
    (void) queryObj.set("params", queryParamsObj);

    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object exclAppInfoObj1;
    (void) exclAppInfoObj1.set("appId", toBase64("appId1"));
    (void) exclAppInfoObj1.set("def", false);
    (void) exclAppInfoObj1.set("description", toBase64("description1"));

    Poco::JSON::Object exclAppInfoObj2;
    (void) exclAppInfoObj2.set("appId", toBase64("appId2"));
    (void) exclAppInfoObj2.set("def", false);
    (void) exclAppInfoObj2.set("description", toBase64("description2"));

    Poco::JSON::Array applicationList;
    applicationList.add(exclAppInfoObj1);
    applicationList.add(exclAppInfoObj2);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("applicationList", applicationList);

    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::EXCLAPP_GETLIST));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto exclAppGetListJob = std::dynamic_pointer_cast<ExclAppGetListJob>(job);
        CPPUNIT_ASSERT(exclAppGetListJob);
        CPPUNIT_ASSERT(!exclAppGetListJob->_default);

        exclAppGetListJob->_applicationList = {ExclusionAppInfo("appId1", "description1", false),
                                               ExclusionAppInfo("appId2", "description2", false)};
    };

    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
}

void TestGuiCommChannel::testExclAppSetListJob() {
    // Query. No need to pass a request id as the response is via a callback.
    Poco::JSON::Object queryObj;
    (void) queryObj.set("num", toInt(RequestNum::EXCLAPP_SETLIST));

    Poco::JSON::Object exclAppInfoObj1;
    (void) exclAppInfoObj1.set("appId", toBase64("appId1"));
    (void) exclAppInfoObj1.set("def", false);
    (void) exclAppInfoObj1.set("description", toBase64("description1"));

    Poco::JSON::Object exclAppInfoObj2;
    (void) exclAppInfoObj2.set("appId", toBase64("appId2"));
    (void) exclAppInfoObj2.set("def", false);
    (void) exclAppInfoObj2.set("description", toBase64("description2"));

    Poco::JSON::Array applicationList;
    applicationList.add(exclAppInfoObj1);
    applicationList.add(exclAppInfoObj2);

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("applicationList", applicationList);
    (void) queryParamsObj.set("default", false);
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
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::EXCLAPP_SETLIST));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto exclAppSetListJob = std::dynamic_pointer_cast<ExclAppSetListJob>(job);
        CPPUNIT_ASSERT(exclAppSetListJob);
        CPPUNIT_ASSERT(!exclAppSetListJob->_default);
        CPPUNIT_ASSERT_EQUAL(size_t{2}, exclAppSetListJob->_applicationList.size());
        CPPUNIT_ASSERT(ExclusionAppInfo("appId1", "description1", false) == exclAppSetListJob->_applicationList.at(0));
        CPPUNIT_ASSERT(ExclusionAppInfo("appId2", "description2", false) == exclAppSetListJob->_applicationList.at(1));
    };

    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
}

void TestGuiCommChannel::testExclAppGetFetchingAppListJob() {
    // Query. No need to pass a request id as the response is via a callback.
    Poco::JSON::Object queryObj;
    (void) queryObj.set("num", toInt(RequestNum::EXCLAPP_GET_FETCHING_APP_LIST));
    const Poco::JSON::Object queryParamsObj;
    (void) queryObj.set("params", queryParamsObj);

    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object applicationTableObj;
    (void) applicationTableObj.set("appId1", toBase64("applicationName1"));
    (void) applicationTableObj.set("appId2", toBase64("applicationName2"));

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("applicationTable", applicationTableObj);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::EXCLAPP_GET_FETCHING_APP_LIST));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto exclAppGetFetchingAppListJob = std::dynamic_pointer_cast<ExclAppGetFetchingAppListJob>(job);
        CPPUNIT_ASSERT(exclAppGetFetchingAppListJob);

        exclAppGetFetchingAppListJob->_applicationTable = {{"appId1", "applicationName1"}, {"appId2", "applicationName2"}};
    };

    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
}

} // namespace KDC
#endif
