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

#include "comm/guijobs/excltemplgetexcludedjob.h"
#include "comm/guijobs/excltemplgetlistjob.h"

#include "testguicommchannel.h"
#include "testcommhelpers.h"

namespace KDC {

using namespace testcommhelpers;

void TestGuiCommChannel::testExclTemplGetExcludedJob() {
    // Query. No need to pass a request id as the response is via a callback.
    Poco::JSON::Object queryObj;
    (void) queryObj.set("num", toInt(RequestNum::EXCLTEMPL_GETEXCLUDED));
    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("name", toBase64("templateName"));
    (void) queryObj.set("params", queryParamsObj);

    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("isExcluded", true);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::EXCLTEMPL_GETEXCLUDED));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto exclTemplGetExcludedJob = std::dynamic_pointer_cast<ExclTemplGetExcludedJob>(job);
        CPPUNIT_ASSERT(exclTemplGetExcludedJob);
        CPPUNIT_ASSERT(CommString{Str("templateName")} == exclTemplGetExcludedJob->_name);

        exclTemplGetExcludedJob->_isExcluded = true;
    };

    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
}

void TestGuiCommChannel::testExclTemplGetListJob() {
    // Query. No need to pass a request id as the response is via a callback.
    Poco::JSON::Object queryObj;
    (void) queryObj.set("num", toInt(RequestNum::EXCLTEMPL_GETLIST));
    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("default", true);
    (void) queryObj.set("params", queryParamsObj);

    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object exclusionTemplateInfo1;
    (void) exclusionTemplateInfo1.set("template", toBase64("template1"));
    (void) exclusionTemplateInfo1.set("warning", true);
    (void) exclusionTemplateInfo1.set("default", true);
    (void) exclusionTemplateInfo1.set("deleted", true);

    Poco::JSON::Object exclusionTemplateInfo2;
    (void) exclusionTemplateInfo2.set("template", toBase64("template2"));
    (void) exclusionTemplateInfo2.set("warning", false);
    (void) exclusionTemplateInfo2.set("default", true);
    (void) exclusionTemplateInfo2.set("deleted", false);

    Poco::JSON::Array exclusionTemplateList;
    exclusionTemplateList.add(exclusionTemplateInfo1);
    exclusionTemplateList.add(exclusionTemplateInfo2);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("exclusionTemplateList", exclusionTemplateList);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::EXCLTEMPL_GETLIST));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto exclTemplGetListJob = std::dynamic_pointer_cast<ExclTemplGetListJob>(job);
        CPPUNIT_ASSERT(exclTemplGetListJob);
        CPPUNIT_ASSERT(exclTemplGetListJob->_default);

        exclTemplGetListJob->_exclusionTemplateList = {ExclusionTemplateInfo("template1", true, true, true),
                                                       ExclusionTemplateInfo("template2", false, true, false)};
    };

    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
}

} // namespace KDC
