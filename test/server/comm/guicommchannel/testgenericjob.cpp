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

#include "testguicommchannel.h"
#include "libcommonserver/log/log.h"

namespace KDC {


void TestGuiCommChannel::testGenericJob(const CommString &query, const CommString &answer, const CommString &cbkAnswer,
                                        const std::function<void(std::shared_ptr<AbstractGuiJob>)> &processFct) {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    CPPUNIT_ASSERT(cbkAnswer.empty());
#else
    CPPUNIT_ASSERT(!cbkAnswer.empty());
#endif

    auto test = [&](const CommString &testQuery, std::shared_ptr<AbstractCommChannel> testChannel) {
        //  Deserialize generic parameters
        int requestId = 0;
        RequestNum requestNum = RequestNum::Unknown;
        Poco::DynamicStruct inParams;
        if (!AbstractGuiJob::deserializeGenericInputParms(testQuery, requestId, requestNum, inParams)) {
            CPPUNIT_ASSERT(false);
        }

        // Create job
        auto job = _guiJobFactory.make(requestNum, nullptr, requestId, inParams, testChannel);
        CPPUNIT_ASSERT(job != nullptr);

        // Deserialize specific parameters
        if (!job->deserializeInputParms()) {
            CPPUNIT_ASSERT(false);
        }

        // Process job simulation
        processFct(job);

        // Serialize specific parameters
        if (!job->serializeOutputParms()) {
            CPPUNIT_ASSERT(false);
        }

        if (!job->serializeGenericOutputParms(ExitCode::Ok)) {
            CPPUNIT_ASSERT(false);
        }

        if (requestNum != RequestNum::USER_INFOLIST) {
            // TODO: Remove this exception when UserInfo._avatar will be a CommBLOB instead of a QImage
            // (QImage.save() gives different results depending on the machine)
            const std::string message =
                    CommonUtility::commString2Str(job->_outputParamsStr.c_str()) + " != " + CommonUtility::commString2Str(answer);
            CPPUNIT_ASSERT_MESSAGE(message, job->_outputParamsStr == answer);
        }

        CPPUNIT_ASSERT(testChannel->sendMessage(job->_outputParamsStr));
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    auto testChannel = std::make_shared<GuiCommChannel>(Poco::Net::StreamSocket());
    test(query, testChannel);
#else
    auto readyReadCbk = [&](std::shared_ptr<AbstractCommChannel> testChannel) {
        if (testChannel->canReadMessage()) {
            CommString testQuery = testChannel->readMessage();
            if (!testQuery.empty()) {
                test(testQuery, testChannel);
            } else {
                CPPUNIT_ASSERT(false);
            }
        }
    };

    auto answerCbk = [=](const CommString &answer) {
        CommString s{answer};
        assert(answer == CommString(cbkAnswer));
        CPPUNIT_ASSERT(answer == CommString(cbkAnswer));
    };

    GuiCommChannel::runProcessQuery(query, readyReadCbk, answerCbk);
#endif
}

} // namespace KDC
