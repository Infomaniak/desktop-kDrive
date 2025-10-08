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
#include "comm/guijobs/abstractguijob.h"
#include "comm/guijobs/loginrequesttokenjob.h"
#include "comm/guijobs/userdbidlistjob.h"
#include "libcommon/comm.h"
#include "log/log.h"

#include "mocks/libcommonserver/db/mockdb.h"
#include "test_utility/testhelpers.h"

namespace KDC {

uint64_t GuiCommChannelTest::readData(CommChar *data, uint64_t maxlen) {
    std::scoped_lock lock(_bufferMutex);
    uint64_t toRead = (std::min)(maxlen, static_cast<uint64_t>(_buffer.size()));
    if (toRead > 0) {
        std::memcpy(data, _buffer.data(), toRead * sizeof(CommChar));
        _buffer.erase(0, toRead);
        return toRead;
    }
    return toRead;
}

uint64_t GuiCommChannelTest::writeData(const CommChar *data, uint64_t len) {
    std::scoped_lock lock(_bufferMutex);
    _buffer.append(data, len);
    return len;
}

uint64_t GuiCommChannelTest::bytesAvailable() const {
    std::scoped_lock lock(_bufferMutex);
    return _buffer.size();
}

void TestGuiCommChannel::setUp() {
    TestBase::start();

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    (void) std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);
}

void TestGuiCommChannel::tearDown() {
    TestBase::stop();
}

void TestGuiCommChannel::testSendMessage() {
    GuiCommChannelTest channelTest;

    CommString message = Str("{\"type\":\"test\",\"content\":\"Hello, World!\"}");
    CPPUNIT_ASSERT(channelTest.sendMessage(message)); // Valid JSON

    message = Str("{\"type\":\"test\",\"content\":\"");
    CPPUNIT_ASSERT(!channelTest.sendMessage(message)); // Invalid JSON

    message = Str(""); // Empty message
    CPPUNIT_ASSERT(!channelTest.sendMessage(message)); // Invalid JSON
}

void TestGuiCommChannel::testReadMessage() {
    std::shared_ptr<GuiCommChannelTest> channelTest = std::make_shared<GuiCommChannelTest>();

    // Read a single JSON
    CommString message[3] = {Str("{\"type\":\"test\",\"content\":\"Hello, World!\"}"),
                             Str("{\"type\":\"test\",\"content\":\"Another message\"}"),
                             Str("{\"type\":\"test\",\"content\":\"Final message\"}")};

    CPPUNIT_ASSERT(channelTest->sendMessage(message[0]));

    CommString readMessage = channelTest->readMessage();
    CPPUNIT_ASSERT(message[0] == readMessage);

    // Read with multiple JSON in buffer
    CPPUNIT_ASSERT(channelTest->sendMessage(message[0]));
    CPPUNIT_ASSERT(channelTest->sendMessage(message[1]));
    CPPUNIT_ASSERT(channelTest->sendMessage(message[2]));
    for (int i = 0; i < 3; ++i) {
        readMessage = channelTest->readMessage();
        CPPUNIT_ASSERT(message[i] == readMessage);
    }

    // Read partial JSON
    CPPUNIT_ASSERT(
            channelTest->writeData(message[0].substr(0, 10).c_str(), 10)); // Partial (use write data to bypass sendMessage check)
    readMessage = channelTest->readMessage();
    CPPUNIT_ASSERT(readMessage.empty()); // Should be empty as JSON is incomplete

    CPPUNIT_ASSERT(channelTest->writeData(message[0].substr(10).c_str(), message[0].size() - 10)); // Complete the JSON
    readMessage = channelTest->readMessage();
    CPPUNIT_ASSERT(message[0] == readMessage); // Now should be complete
}

void TestGuiCommChannel::testCanReadMessage() {
    GuiCommChannelTest channelTest;
    CommString message = Str("{\"type\":\"test\",\"content\":\"Hello, World!\"}");
    CPPUNIT_ASSERT(channelTest.sendMessage(message)); // Valid JSON
    CPPUNIT_ASSERT(channelTest.canReadMessage()); // Should be able to read a message
    CommString readMessage = channelTest.readMessage();
    CPPUNIT_ASSERT(message == readMessage);
    CPPUNIT_ASSERT(!channelTest.canReadMessage()); // No more messages to read

    // Send partial JSON
    CPPUNIT_ASSERT(
            channelTest.writeData(message.substr(0, 10).c_str(), 10)); // Partial (use write data to bypass sendMessage check)
    CPPUNIT_ASSERT(!channelTest.canReadMessage()); // Should not be able to read a message
    CPPUNIT_ASSERT(channelTest.writeData(message.substr(10).c_str(), message.size() - 10)); // Complete the JSON
    CPPUNIT_ASSERT(channelTest.canReadMessage()); // Now should be able to read a message
}

void TestGuiCommChannel::testLoginRequestTokenJob() {
    // Job expected answer
    const auto answerStr{Str(R"({ "cause": 0,)"
                             R"( "code": 0,)"
                             R"( "id": 1,)"
                             R"( "num": 1,)" // RequestNum::LOGIN_REQUESTTOKEN
                             R"( "params": {)"
                             R"( "userDbId": 1 },)"
                             R"( "type": 1 })")}; // GuiJobType::Query

    auto test = [&](const CommString &query, const CommString &answer, std::shared_ptr<AbstractCommChannel> channel) {
        //  Deserialize generic parameters
        int requestId = 0;
        RequestNum requestNum = RequestNum::Unknown;
        Poco::DynamicStruct inParams;
        if (!AbstractGuiJob::deserializeGenericInputParms(query, requestId, requestNum, inParams)) {
            CPPUNIT_ASSERT(false);
        }

        CPPUNIT_ASSERT(requestId == 1);
        CPPUNIT_ASSERT(requestNum == RequestNum::LOGIN_REQUESTTOKEN);
        CPPUNIT_ASSERT(inParams.size() == 2);
        CPPUNIT_ASSERT(inParams["code"] == "YWFhYQ==");
        CPPUNIT_ASSERT(inParams["codeVerifier"] == "YmJiYg==");

        // Create job
        auto job = _guiJobFactory.make(requestNum, nullptr, requestId, inParams, channel);
        CPPUNIT_ASSERT(job != nullptr);

        // Deserialize specific parameters
        if (!job->deserializeInputParms()) {
            CPPUNIT_ASSERT(false);
        }

        auto loginRequestTokenJob = std::dynamic_pointer_cast<LoginRequestTokenJob>(job);
        CPPUNIT_ASSERT(loginRequestTokenJob->_code == Str("aaaa"));
        CPPUNIT_ASSERT(loginRequestTokenJob->_codeVerifier == Str("bbbb"));

        // Process job simulation
        loginRequestTokenJob->_userDbId = 1;
        job->setExitInfo(ExitCode::Ok);

        // Serialize specific parameters
        if (!job->serializeOutputParms()) {
            CPPUNIT_ASSERT(false);
        }

        CPPUNIT_ASSERT(job->_outParams["userDbId"] == 1);

        if (!job->serializeGenericOutputParms(ExitCode::Ok)) {
            CPPUNIT_ASSERT(false);
        }

        CPPUNIT_ASSERT(job->_outputParamsStr == answer);

        CPPUNIT_ASSERT(channel->sendMessage(job->_outputParamsStr));
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{Str(R"({ "id": 1,)"
                            R"( "num": 1,)" // RequestNum::LOGIN_REQUESTTOKEN
                            R"( "params": {)"
                            R"( "code": "YWFhYQ==",)" // "aaaa" base64 encoded
                            R"( "codeVerifier": "YmJiYg==" } })")}; // "bbbb" base64 encoded

    auto channel = std::make_shared<GuiCommChannel>(Poco::Net::StreamSocket());
    test(queryStr, answerStr, channel);
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{Str(R"({ "num": 1,)" // RequestNum::LOGIN_REQUESTTOKEN
                            R"( "params": {)"
                            R"( "code": "YWFhYQ==",)" // "aaaa" base64 encoded
                            R"( "codeVerifier": "YmJiYg==" } })")}; // "bbbb" base64 encoded

    // Callback expected answer
    const auto cbkAnswerStr{Str(R"({"cause":0,"code":0,"id":1,"params":{"userDbId":1}})")};

    auto readyReadCbk = [&](std::shared_ptr<AbstractCommChannel> channel) {
        if (channel->canReadMessage()) {
            CommString query = channel->readMessage();
            if (!query.empty()) {
                test(query, answerStr, channel);
            } else {
                CPPUNIT_ASSERT(false);
            }
        }
    };

    auto answerCbk = [=](const CommString &answer) { CPPUNIT_ASSERT(answer == CommString(cbkAnswerStr)); };

    GuiCommChannel::runSendQuery(queryStr, readyReadCbk, answerCbk);
#endif
}

void TestGuiCommChannel::testUserDbIdListJob() {
    // Job expected answer
    const auto answerStr{Str(R"({ "cause": 0,)"
                             R"( "code": 0,)"
                             R"( "id": 1,)"
                             R"( "num": 2,)" // RequestNum::USER_DBIDLIST
                             R"( "params": {)"
                             R"( "userDbIdList": [ 1, 2, 3 ] },)"
                             R"( "type": 1 })")}; // GuiJobType::Query

    auto test = [&](const CommString &query, const CommString &answer, std::shared_ptr<AbstractCommChannel> channel) {
        //  Deserialize generic parameters
        int requestId = 0;
        RequestNum requestNum = RequestNum::Unknown;
        Poco::DynamicStruct inParams;
        if (!AbstractGuiJob::deserializeGenericInputParms(query, requestId, requestNum, inParams)) {
            CPPUNIT_ASSERT(false);
        }

        CPPUNIT_ASSERT(requestId == 1);
        CPPUNIT_ASSERT(requestNum == RequestNum::USER_DBIDLIST);
        CPPUNIT_ASSERT(inParams.size() == 0);

        // Create job
        auto job = _guiJobFactory.make(requestNum, nullptr, requestId, inParams, channel);
        CPPUNIT_ASSERT(job != nullptr);

        // Deserialize specific parameters
        if (!job->deserializeInputParms()) {
            CPPUNIT_ASSERT(false);
        }

        auto userDbIdListJob = std::dynamic_pointer_cast<UserDbIdListJob>(job);

        // Process job simulation
        userDbIdListJob->_userDbIdList = {1, 2, 3};
        job->setExitInfo(ExitCode::Ok);

        // Serialize specific parameters
        if (!job->serializeOutputParms()) {
            CPPUNIT_ASSERT(false);
        }

        CPPUNIT_ASSERT(job->_outParams["userDbIdList"] == "[ 1, 2, 3 ]");

        if (!job->serializeGenericOutputParms(ExitCode::Ok)) {
            CPPUNIT_ASSERT(false);
        }

        CPPUNIT_ASSERT(job->_outputParamsStr == answer);

        CPPUNIT_ASSERT(channel->sendMessage(job->_outputParamsStr));
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{Str(R"({ "id": 1,)"
                            R"( "num": 2,)" // RequestNum::USER_DBIDLIST
                            R"( "params": { } })")};

    auto channel = std::make_shared<GuiCommChannel>(Poco::Net::StreamSocket());
    test(queryStr, answerStr, channel);
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{Str(R"({ "num": 2,)" // RequestNum::USER_DBIDLIST
                            R"( "params": { } })")};

    // Callback expected answer
    const auto cbkAnswerStr{Str(R"({"cause":0,"code":0,"id":1,"params":{"userDbIdList":[1,2,3]}})")};

    auto readyReadCbk = [&](std::shared_ptr<AbstractCommChannel> channel) {
        if (channel->canReadMessage()) {
            CommString query = channel->readMessage();
            if (!query.empty()) {
                test(query, answerStr, channel);
            } else {
                CPPUNIT_ASSERT(false);
            }
        }
    };

    auto answerCbk = [=](const CommString &answer) { CPPUNIT_ASSERT(answer == CommString(cbkAnswerStr)); };

    GuiCommChannel::runSendQuery(queryStr, readyReadCbk, answerCbk);
#endif
}

} // namespace KDC
