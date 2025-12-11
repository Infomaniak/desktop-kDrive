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
#include "comm/guijobs/errorinfolistjob.h"

namespace KDC {

using namespace testcommhelpers;


void TestGuiCommChannel::testErrorInfoListJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::ERROR_INFOLIST));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("limit", 0);
    (void) queryObj.set("params", queryParamsObj);

    const auto queryStr = testcommhelpers::stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);


    Poco::JSON::Object errorInfoObj;
    (void) errorInfoObj.set("dbId", 1);
    (void) errorInfoObj.set("time", 1000);
    (void) errorInfoObj.set("level", toInt(ErrorLevel::SyncPal));
    std::string b64str;
    CommonUtility::convertToBase64Str("func1", b64str);
    (void) errorInfoObj.set("functionName", b64str);
    (void) errorInfoObj.set("syncDbId", 10);
    CommonUtility::convertToBase64Str("worker1", b64str);
    (void) errorInfoObj.set("workerName", b64str);
    (void) errorInfoObj.set("exitCode", toInt(ExitCode::DataError));
    (void) errorInfoObj.set("exitCause", toInt(ExitCause::DbEntryNotFound));
    CommonUtility::convertToBase64Str("local1", b64str);
    (void) errorInfoObj.set("localNodeId", b64str);
    CommonUtility::convertToBase64Str("remote1", b64str);
    (void) errorInfoObj.set("remoteNodeId", b64str);
    (void) errorInfoObj.set("nodeType", toInt(NodeType::Unknown));
    CommonUtility::convertToBase64Str("path1", b64str);
    (void) errorInfoObj.set("path", b64str);
    (void) errorInfoObj.set("destinationPath", "");
    (void) errorInfoObj.set("conflictType", toInt(ConflictType::None));
    (void) errorInfoObj.set("inconsistencyType", toInt(InconsistencyType::None));
    (void) errorInfoObj.set("cancelType", toInt(CancelType::None));
    (void) errorInfoObj.set("autoResolved", false);

    Poco::JSON::Array errorInfoListArr;
    (void) errorInfoListArr.add(errorInfoObj);

    Poco::JSON::Object answerParamsObj;
    (void) answerParamsObj.set("errorInfoList", errorInfoListArr);
    (void) answerObj.set("params", answerParamsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::ERROR_INFOLIST));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = testcommhelpers::stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto errorJob = std::dynamic_pointer_cast<ErrorInfolistJob>(job);
        CPPUNIT_ASSERT(errorJob);
        ErrorInfo e1;
        e1.setDbId(1);
        e1.setTime(1000);
        e1.setLevel(ErrorLevel::SyncPal);
        e1.setFunctionName("func1");
        e1.setSyncDbId(10);
        e1.setWorkerName("worker1");
        e1.setExitCode(ExitCode::DataError);
        e1.setExitCause(ExitCause::DbEntryNotFound);
        e1.setLocalNodeId("local1");
        e1.setRemoteNodeId("remote1");
        e1.setNodeType(NodeType::Unknown);
        e1.setPath("path1");
        e1.setConflictType(ConflictType::None);
        e1.setInconsistencyType(InconsistencyType::None);
        e1.setCancelType(CancelType::None);
        e1.setDestinationPath("");
        e1.setAutoResolved(false);
        errorJob->_errorInfoList = {e1};
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

} // namespace KDC
