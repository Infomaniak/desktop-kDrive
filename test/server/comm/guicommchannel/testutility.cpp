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

#include "comm/guijobs/utilitycheckcommstatusjob.h"

#include "testguicommchannel.h"
#include "../testcommhelpers.h"

namespace KDC {

using namespace testcommhelpers;

void TestGuiCommChannel::testUtilityCheckCommStatusJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::UTILITY_CHECKCOMMSTATUS));
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
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::UTILITY_CHECKCOMMSTATUS));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto utilityCheckCommStatusJob = std::dynamic_pointer_cast<UtilityCheckCommStatusJob>(job);
        CPPUNIT_ASSERT(utilityCheckCommStatusJob);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

} // namespace KDC
