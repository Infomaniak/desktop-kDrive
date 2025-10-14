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
#include "comm/guijobs/userinfolistjob.h"
#include "comm/guijobs/useravailabledrivesjob.h"
#include "libcommon/comm.h"
#include "log/log.h"

#include "mocks/libcommonserver/db/mockdb.h"
#include "test_utility/testhelpers.h"

#include <qbytearray.h>
#include <qbuffer.h>

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
    // Base64 conversions
    // "aaaa" <=> "YWFhYQ=="
    // "bbbb" <=> "YmJiYg=="

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{Str(R"({ "id": 1,)"
                            R"( "num": 1,)" // RequestNum::LOGIN_REQUESTTOKEN
                            R"( "params": {)"
                            R"( "code": "YWFhYQ==",)"
                            R"( "codeVerifier": "YmJiYg==" } })")};

#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{Str(R"({ "num": 1,)" // RequestNum::LOGIN_REQUESTTOKEN
                            R"( "params": {)"
                            R"( "code": "YWFhYQ==",)"
                            R"( "codeVerifier": "YmJiYg==" } })")};

    // Callback expected answer
    const auto cbkAnswerStr{Str(R"({"cause":0,"code":0,"id":1,"params":{"userDbId":1}})")};
#endif

    // Job expected answer
    const auto answerStr{Str(R"({ "cause": 0,)"
                             R"( "code": 0,)"
                             R"( "id": 1,)"
                             R"( "num": 1,)" // RequestNum::LOGIN_REQUESTTOKEN
                             R"( "params": {)"
                             R"( "userDbId": 1 },)"
                             R"( "type": 1 })")}; // GuiJobType::Query

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto loginRequestTokenJob = std::dynamic_pointer_cast<LoginRequestTokenJob>(job);
        CPPUNIT_ASSERT(loginRequestTokenJob->_code == Str("aaaa"));
        CPPUNIT_ASSERT(loginRequestTokenJob->_codeVerifier == Str("bbbb"));

        // Process job simulation
        loginRequestTokenJob->_userDbId = 1;
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUserDbIdListJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{Str(R"({ "id": 1,)"
                            R"( "num": 2,)" // RequestNum::USER_DBIDLIST
                            R"( "params": { } })")};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{Str(R"({ "num": 2,)" // RequestNum::USER_DBIDLIST
                            R"( "params": { } })")};

    // Callback expected answer
    const auto cbkAnswerStr{Str(R"({"cause":0,"code":0,"id":1,"params":{"userDbIdList":[1,2,3]}})")};
#endif

    // Job expected answer
    const auto answerStr{Str(R"({ "cause": 0,)"
                             R"( "code": 0,)"
                             R"( "id": 1,)"
                             R"( "num": 2,)" // RequestNum::USER_DBIDLIST
                             R"( "params": {)"
                             R"( "userDbIdList": [ 1, 2, 3 ] },)"
                             R"( "type": 1 })")}; // GuiJobType::Query

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto userDbIdListJob = std::dynamic_pointer_cast<UserDbIdListJob>(job);

        userDbIdListJob->_userDbIdList = {1, 2, 3};
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUserInfoListJob() {
    // Base64 conversions
    // "aaaaa" <=> "YYWFhYWE="
    // "aaaaa@xxx.com" <=> "YWFhYWFAeHh4LmNvbQ=="
    // "bbbbb" <=> "YmJiYmI="
    // "bbbbb@xxx.com" <=> "YmJiYmJAeHh4LmNvbQ=="

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{Str(R"({ "id": 1,)"
                            R"( "num": 3,)" // RequestNum::USER_INFOLIST
                            R"( "params": { } })")};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{Str(R"({ "num": 3,)" // RequestNum::USER_INFOLIST
                            R"( "params": { } })")};

    // Callback expected answer
    const auto cbkAnswerStr{Str(
            R"({"cause":0,"code":0,"id":1,"params":{"userInfoList":[)"
            R"({"avatar":"iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAABlBMVEUAAAD///+l2Z/dAAAAAXRSTlMAQObYZgAAAAlwSFlzAAAPYQAAD2EBqD+naQAAAApJREFUCJljYAAAAAIAAfRxZKYAAAAASUVORK5CYII=","connected":true,"dbId":1,"email":"YWFhYWFAeHh4LmNvbQ==","isStaff":false,"name":"YWFhYWE=","userId":1001},)"
            R"({"avatar":"iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAABlBMVEUAAAD///+l2Z/dAAAAAXRSTlMAQObYZgAAAAlwSFlzAAAPYQAAD2EBqD+naQAAAApJREFUCJljYAAAAAIAAfRxZKYAAAAASUVORK5CYII=","connected":false,"dbId":2,"email":"YmJiYmJAeHh4LmNvbQ==","isStaff":false,"name":"YmJiYmI=","userId":1002}]}})")};
#endif

    // Job expected answer
    const auto answerStr{
            Str(R"({ "cause": 0,)"
                R"( "code": 0,)"
                R"( "id": 1,)"
                R"( "num": 3,)" // RequestNum::USER_INFOLIST
                R"( "params": {)"
                R"( "userInfoList": [)"
                R"( { "avatar": "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAABlBMVEUAAAD///+l2Z/dAAAAAXRSTlMAQObYZgAAAAlwSFlzAAAPYQAAD2EBqD+naQAAAApJREFUCJljYAAAAAIAAfRxZKYAAAAASUVORK5CYII=", "connected": true, "dbId": 1, "email": "YWFhYWFAeHh4LmNvbQ==", "isStaff": false, "name": "YWFhYWE=", "userId": 1001 },)"
                R"( { "avatar": "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAABlBMVEUAAAD///+l2Z/dAAAAAXRSTlMAQObYZgAAAAlwSFlzAAAPYQAAD2EBqD+naQAAAApJREFUCJljYAAAAAIAAfRxZKYAAAAASUVORK5CYII=", "connected": false, "dbId": 2, "email": "YmJiYmJAeHh4LmNvbQ==", "isStaff": false, "name": "YmJiYmI=", "userId": 1002 } ] },)"
                R"( "type": 1 })") // GuiJobType::Query
    };

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto userInfoListJob = std::dynamic_pointer_cast<UserInfoListJob>(job);
        std::string avatarBase64Str{
                R"(iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAABlBMVEUAAAD///+l2Z/dAAAAAXRSTlMAQObYZgAAAAlwSFlzAAAPYQAAD2EBqD+naQAAAApJREFUCJljYAAAAAIAAfRxZKYAAAAASUVORK5CYII=)"};
        CommBLOB avatarBLOB;
        CommonUtility::convertFromBase64Str(avatarBase64Str, avatarBLOB);
        QByteArray avatarQBA;
        std::copy(avatarBLOB.begin(), avatarBLOB.end(), std::back_inserter(avatarQBA));
        QImage avatar;
        (void) avatar.loadFromData(avatarQBA);

        UserInfo ui1(1, 1001, "aaaaa", "aaaaa@xxx.com", avatar, true);
        UserInfo ui2(2, 1002, "bbbbb", "bbbbb@xxx.com", avatar, false);

        userInfoListJob->_userInfoList = {ui1, ui2};
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUserDeleteJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{Str(R"({ "id": 1,)"
                            R"( "num": 4,)" // RequestNum::USER_DELETE
                            R"( "params": { "userDbId": 1 } })")};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{Str(R"({ "num": 4,)" // RequestNum::USER_DELETE
                            R"( "params": { "userDbId": 1 } })")};

    // Callback expected answer
    const auto cbkAnswerStr{Str(R"({"cause":0,"code":0,"id":1,"params":{}})")};
#endif

    // Job expected answer
    const auto answerStr{Str(R"({ "cause": 0,)"
                             R"( "code": 0,)"
                             R"( "id": 1,)"
                             R"( "num": 4,)" // RequestNum::USER_DELETE
                             R"( "params": {  },)"
                             R"( "type": 1 })")}; // GuiJobType::Query

    auto processFct = [](std::shared_ptr<AbstractGuiJob>) {};

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUserAvailableDrivesJob() {
    // Base64 conversions
    // "#aabbcc" <=> "I2FhYmJjYw=="
    // "#ddeeff" <=> "I2RkZWVmZg=="
    // "drive1111" <=> "ZHJpdmUxMTEx"
    // "drive2222" <=> "ZHJpdmUyMjIy"

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{Str(R"({ "id": 1,)"
                            R"( "num": 5,)" // RequestNum::USER_AVAILABLEDRIVES
                            R"( "params": { "userDbId": 1 } })")};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{Str(R"({ "num": 5,)" // RequestNum::USER_AVAILABLEDRIVES
                            R"( "params": { "userDbId": 1 } })")};

    // Callback expected answer
    const auto cbkAnswerStr{
            Str(R"({"cause":0,"code":0,"id":1,"params":{"driveAvailableInfoList":[)"
                R"({"accountId":11,"color":"I2FhYmJjYw==","driveId":1111,"name":"ZHJpdmUxMTEx","userDbId":1,"userId":111},)"
                R"({"accountId":22,"color":"I2RkZWVmZg==","driveId":2222,"name":"ZHJpdmUyMjIy","userDbId":2,"userId":222}]}})")};
#endif

    // Job expected answer
    const auto answerStr{Str(
            R"({ "cause": 0,)"
            R"( "code": 0,)"
            R"( "id": 1,)"
            R"( "num": 5,)" // RequestNum::USER_AVAILABLEDRIVES
            R"( "params": { "driveAvailableInfoList": [)"
            R"( { "accountId": 11, "color": "I2FhYmJjYw==", "driveId": 1111, "name": "ZHJpdmUxMTEx", "userDbId": 1, "userId": 111 },)"
            R"( { "accountId": 22, "color": "I2RkZWVmZg==", "driveId": 2222, "name": "ZHJpdmUyMjIy", "userDbId": 2, "userId": 222 } ] },)"
            R"( "type": 1 })")}; // GuiJobType::Query

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto userAvailableDrivesJob = std::dynamic_pointer_cast<UserAvailableDrivesJob>(job);

        DriveAvailableInfo dai1(1111, 111, 11, "drive1111", "#aabbcc");
        dai1.setUserDbId(1);
        DriveAvailableInfo dai2(2222, 222, 22, "drive2222", "#ddeeff");
        dai2.setUserDbId(2);

        userAvailableDrivesJob->_driveAvailableInfoList = {dai1, dai2};
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testGenericJob(const CommString &query, const CommString &answer, const CommString &cbkAnswer,
                                        const std::function<void(std::shared_ptr<AbstractGuiJob>)> &processFct) {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    assert(cbkAnswer.empty());
#else
    assert(!cbkAnswer.empty());
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

        if (requestNum == RequestNum::USER_INFOLIST) {
            // TODO: Use the general case when UserInfo._avatar will be a CommBLOB instead of a QImage
            // (QImage.save() gives different results depending on the machine)
            CPPUNIT_ASSERT(job->_outputParamsStr.size() == answer.size());
        } else {
            CPPUNIT_ASSERT(job->_outputParamsStr == answer);
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

    auto answerCbk = [=](const CommString &answer) { CPPUNIT_ASSERT(answer == CommString(cbkAnswer)); };

    GuiCommChannel::runSendQuery(query, readyReadCbk, answerCbk);
#endif
}

} // namespace KDC
