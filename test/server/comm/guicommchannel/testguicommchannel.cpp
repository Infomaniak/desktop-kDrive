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

#include "comm/guijobs/abstractguijob.h"
#include "comm/guijobs/loginrequesttokenjob.h"
#include "comm/guijobs/userdbidlistjob.h"
#include "comm/guijobs/userinfolistjob.h"
#include "comm/guijobs/useravailabledrivesjob.h"
#include "comm/guijobs/accountinfolistjob.h"
#include "comm/guijobs/driveinfolistjob.h"
#include "comm/guijobs/drivesearchjob.h"
#include "comm/guijobs/driveupdatejob.h"

#include "libcommon/comm.h"
#include "libcommon/log/log.h"

#include "mocks/libcommonserver/db/mockdb.h"
#include "test_utility/testhelpers.h"

#include <qbytearray.h>

namespace KDC {

using namespace testcommhelpers;

uint64_t GuiCommChannelTest::readData(CommChar *data, uint64_t maxlen) {
    std::scoped_lock lock(_bufferMutex);
    uint64_t toRead = (std::min) (maxlen, static_cast<uint64_t>(_buffer.size()));
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
    (void) IoHelper::deleteItem(parmsDbPath);
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

void TestGuiCommChannel::testContainsCompleteMessage() {
    GuiCommChannelTest channelTest;
    size_t endIndex = 0;

    // Empty message
    CPPUNIT_ASSERT(!channelTest.containsCompleteMessage(Str(""), endIndex));

    // The message should start with a "{" or "["
    CPPUNIT_ASSERT(!channelTest.containsCompleteMessage(Str("qwertz"), endIndex));

    // The message should be correctly parenthesized
    CPPUNIT_ASSERT(!channelTest.containsCompleteMessage(Str("{\"var\":\"value\")"), endIndex));
    CPPUNIT_ASSERT(channelTest.containsCompleteMessage(Str("{\"var\":\"value\"})"), endIndex) && endIndex == 14);
    CPPUNIT_ASSERT(channelTest.containsCompleteMessage(Str("{\"var\":\"value\"}{\"var2\":\"value2\"})"), endIndex) &&
                   endIndex == 14);
    CPPUNIT_ASSERT(!channelTest.containsCompleteMessage(Str("{\"varList\":[1,2,3,4)"), endIndex));
    CPPUNIT_ASSERT(channelTest.containsCompleteMessage(Str("{\"varList\":[1,2,3,4]})"), endIndex) && endIndex == 20);
    CPPUNIT_ASSERT(channelTest.containsCompleteMessage(Str("{{[][]}{[{{[]}{}}]}{{}{}{}}})"), endIndex) && endIndex == 27);
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
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::LOGIN_REQUESTTOKEN)) +
                        R"(,)"
                        R"( "params": {)"
                        R"( "code": "YWFhYQ==",)"
                        R"( "codeVerifier": "YmJiYg==" } })"};

#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::LOGIN_REQUESTTOKEN)) +
                        R"(,)"
                        R"( "params": {)"
                        R"( "code": "YWFhYQ==",)"
                        R"( "codeVerifier": "YmJiYg==" } })"};

    // Callback expected answer
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{"userDbId":1}})"};
#endif

    // Job expected answer
    const auto answerStr{R"({ "cause": 0,)"
                         R"( "code": 0,)"
                         R"( "id": 1,)"
                         R"( "num": )" +
                         std::to_string(toInt(RequestNum::LOGIN_REQUESTTOKEN)) +
                         R"(,)"
                         R"( "params": {)"
                         R"( "userDbId": 1 },)"
                         R"( "type": )" +
                         std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto loginRequestTokenJob = std::dynamic_pointer_cast<LoginRequestTokenJob>(job);
        CPPUNIT_ASSERT(loginRequestTokenJob->_code == Str("aaaa"));
        CPPUNIT_ASSERT(loginRequestTokenJob->_codeVerifier == Str("bbbb"));

        // Process job simulation
        loginRequestTokenJob->_userDbId = 1;
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUserDbIdListJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::USER_DBIDLIST)) +
                        R"(,)"
                        R"( "params": { } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::USER_DBIDLIST)) +
                        R"(,)"
                        R"( "params": { } })"};

    // Callback expected answer
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{"userDbIdList":[1,2,3]}})"};
#endif

    // Job expected answer
    const auto answerStr{R"({ "cause": 0,)"
                         R"( "code": 0,)"
                         R"( "id": 1,)"
                         R"( "num": )" +
                         std::to_string(toInt(RequestNum::USER_DBIDLIST)) +
                         R"(,)"
                         R"( "params": {)"
                         R"( "userDbIdList": [ 1, 2, 3 ] },)"
                         R"( "type": )" +
                         std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto userDbIdListJob = std::dynamic_pointer_cast<UserDbIdListJob>(job);

        userDbIdListJob->_userDbIdList = {1, 2, 3};
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testUserInfoListJob() {
    const std::string avatarBase64StdStr{
            R"(iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAABlBMVEUAAAD///+l2Z/dAAAAAXRSTlMAQObYZgAAAAlwSFlzAAAPYQAAD2EBqD+naQAAAApJREFUCJljYAAAAAIAAfRxZKYAAAAASUVORK5CYII=)"};

    // Query
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::USER_INFOLIST));
    const Poco::JSON::Object queryParamsObj;
    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object userInfoObj1;
    (void) userInfoObj1.set("avatar", avatarBase64StdStr);
    (void) userInfoObj1.set("dbId", 1);
    (void) userInfoObj1.set("email", toBase64(Str("aaaaa@xxx.com")));
    (void) userInfoObj1.set("isConnected", true);
    (void) userInfoObj1.set("isStaff", false);
    (void) userInfoObj1.set("name", toBase64(Str("aaaaa")));
    (void) userInfoObj1.set("userId", 1001);

    Poco::JSON::Object userInfoObj2;
    (void) userInfoObj2.set("avatar", avatarBase64StdStr);
    (void) userInfoObj2.set("dbId", 2);
    (void) userInfoObj2.set("email", toBase64(Str("bbbbb@xxx.com")));
    (void) userInfoObj2.set("isConnected", false);
    (void) userInfoObj2.set("isStaff", false);
    (void) userInfoObj2.set("name", toBase64(Str("bbbbb")));
    (void) userInfoObj2.set("userId", 1002);

    Poco::JSON::Array userInfoListObj;
    (void) userInfoListObj.add(userInfoObj1);
    (void) userInfoListObj.add(userInfoObj2);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("userInfoList", userInfoListObj);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::USER_INFOLIST));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);

    auto processFct = [avatarBase64StdStr](std::shared_ptr<AbstractGuiJob> job) {
        auto userInfoListJob = std::dynamic_pointer_cast<UserInfoListJob>(job);
        CPPUNIT_ASSERT(userInfoListJob);

        CommBLOB avatarBLOB;
        CommonUtility::convertFromBase64Str(avatarBase64StdStr, avatarBLOB);
        QByteArray avatarQBA;
        (void) std::copy(avatarBLOB.begin(), avatarBLOB.end(), std::back_inserter(avatarQBA));
        QImage avatar;
        (void) avatar.loadFromData(avatarQBA);

        const UserInfo ui1(1, 1001, "aaaaa", "aaaaa@xxx.com", avatar, true);
        const UserInfo ui2(2, 1002, "bbbbb", "bbbbb@xxx.com", avatar, false);

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
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::USER_DELETE)) +
                        R"(,)"
                        R"( "params": { "userDbId": 1 } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::USER_DELETE)) +
                        R"(,)"
                        R"( "params": { "userDbId": 1 } })"};

    // Callback expected answer
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{}})"};
#endif

    // Job expected answer
    const auto answerStr{R"({ "cause": 0,)"
                         R"( "code": 0,)"
                         R"( "id": 1,)"
                         R"( "num": )" +
                         std::to_string(toInt(RequestNum::USER_DELETE)) +
                         R"(,)"
                         R"( "params": {  },)"
                         R"( "type": )" +
                         std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob>) {
        // No output parameters
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
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
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::USER_AVAILABLEDRIVES)) +
                        R"(,)"
                        R"( "params": { "userDbId": 1 } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::USER_AVAILABLEDRIVES)) +
                        R"(,)"
                        R"( "params": { "userDbId": 1 } })"};

    // Callback expected answer
    const auto cbkAnswerStr{
            R"({"cause":0,"code":0,"id":1,"params":{"driveAvailableInfoList":[)"
            R"({"accountId":11,"color":"I2FhYmJjYw==","driveId":1111,"name":"ZHJpdmUxMTEx","userDbId":1,"userId":111},)"
            R"({"accountId":22,"color":"I2RkZWVmZg==","driveId":2222,"name":"ZHJpdmUyMjIy","userDbId":2,"userId":222}]}})"};
#endif

    // Job expected answer
    const auto answerStr{
            R"({ "cause": 0,)"
            R"( "code": 0,)"
            R"( "id": 1,)"
            R"( "num": )" +
            std::to_string(toInt(RequestNum::USER_AVAILABLEDRIVES)) +
            R"(,)"
            R"( "params": { "driveAvailableInfoList": [)"
            R"( { "accountId": 11, "color": "I2FhYmJjYw==", "driveId": 1111, "name": "ZHJpdmUxMTEx", "userDbId": 1, "userId": 111 },)"
            R"( { "accountId": 22, "color": "I2RkZWVmZg==", "driveId": 2222, "name": "ZHJpdmUyMjIy", "userDbId": 2, "userId": 222 } ] },)"
            R"( "type": )" +
            std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto userAvailableDrivesJob = std::dynamic_pointer_cast<UserAvailableDrivesJob>(job);

        DriveAvailableInfo dai1(1111, 111, 11, "drive1111", "#aabbcc");
        dai1.setUserDbId(1);
        DriveAvailableInfo dai2(2222, 222, 22, "drive2222", "#ddeeff");
        dai2.setUserDbId(2);

        userAvailableDrivesJob->_driveAvailableInfoList = {dai1, dai2};
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testAccountInfoListJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::ACCOUNT_INFOLIST)) +
                        R"(,)"
                        R"( "params": { } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::ACCOUNT_INFOLIST)) +
                        R"(,)"
                        R"( "params": { } })"};

    // Callback expected answer
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{"accountInfoList":[)"
                            R"({"accountId":1111,"dbId":1,"userDbId":1},)"
                            R"({"accountId":2222,"dbId":2,"userDbId":1}]}})"};
#endif

    // Job expected answer
    const auto answerStr{R"({ "cause": 0,)"
                         R"( "code": 0,)"
                         R"( "id": 1,)"
                         R"( "num": )" +
                         std::to_string(toInt(RequestNum::ACCOUNT_INFOLIST)) +
                         R"(,)"
                         R"( "params": {)"
                         R"( "accountInfoList": [)"
                         R"( { "accountId": 1111, "dbId": 1, "userDbId": 1 },)"
                         R"( { "accountId": 2222, "dbId": 2, "userDbId": 1 } ] },)"
                         R"( "type": )" +
                         std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto accountInfoListJob = std::dynamic_pointer_cast<AccountInfoListJob>(job);

        AccountInfo ai1(1, 1);
        ai1.setAccountId(1111);
        AccountInfo ai2(2, 1);
        ai2.setAccountId(2222);

        accountInfoListJob->_accountInfoList = {ai1, ai2};
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

namespace {
std::vector<DriveInfo> createDriveInfoList() {
    DriveInfo di1;
    di1.setDbId(1);
    di1.setId(1111);
    di1.setAccountDbId(1);
    di1.setName("drive1111");
    di1.setColor("#aabbcc");
    di1.setNotifications(true);
    di1.setAdmin(true);
    di1.setMaintenance(false);
    di1.setLocked(false);
    di1.setAccessDenied(false);
    di1.setSize(1000000000);
    di1.setUsedSize(50000000);
    di1.setPackIsFree(true);

    DriveInfo di2;
    di2.setDbId(2);
    di2.setId(2222);
    di2.setAccountDbId(1);
    di2.setName("drive2222");
    di2.setColor("#ddeeff");
    di2.setNotifications(false);
    di2.setAdmin(false);
    di2.setMaintenance(true);
    di2.setLocked(true);
    di2.setAccessDenied(true);
    di2.setSize(2000000000);
    di2.setUsedSize(60000000);
    di2.setPackIsFree(false);

    return {di1, di2};
}

Poco::JSON::Array createDriveInfoObjList() {
    Poco::JSON::Object driveInfoObj1;
    (void) driveInfoObj1.set("accessDenied", false);
    (void) driveInfoObj1.set("accountDbId", 1);
    (void) driveInfoObj1.set("admin", true);
    (void) driveInfoObj1.set("color", toBase64(Str("#aabbcc")));
    (void) driveInfoObj1.set("dbId", 1);
    (void) driveInfoObj1.set("id", 1111);
    (void) driveInfoObj1.set("locked", false);
    (void) driveInfoObj1.set("maintenance", false);
    (void) driveInfoObj1.set("name", toBase64(Str("drive1111")));
    (void) driveInfoObj1.set("notifications", true);
    (void) driveInfoObj1.set("size", 1000000000);
    (void) driveInfoObj1.set("usedSize", 50000000);
    (void) driveInfoObj1.set("isFree", true);

    Poco::JSON::Object driveInfoObj2;
    (void) driveInfoObj2.set("accessDenied", true);
    (void) driveInfoObj2.set("accountDbId", 1);
    (void) driveInfoObj2.set("admin", false);
    (void) driveInfoObj2.set("color", toBase64(Str("#ddeeff")));
    (void) driveInfoObj2.set("dbId", 2);
    (void) driveInfoObj2.set("id", 2222);
    (void) driveInfoObj2.set("locked", true);
    (void) driveInfoObj2.set("maintenance", true);
    (void) driveInfoObj2.set("name", toBase64(Str("drive2222")));
    (void) driveInfoObj2.set("notifications", false);
    (void) driveInfoObj2.set("size", 2000000000);
    (void) driveInfoObj2.set("usedSize", 60000000);
    (void) driveInfoObj2.set("isFree", false);

    Poco::JSON::Array driveInfoObjList;

    (void) driveInfoObjList.add(driveInfoObj1);
    (void) driveInfoObjList.add(driveInfoObj2);

    return driveInfoObjList;
}

} // namespace

void TestGuiCommChannel::testDriveInfoListJob() {
    // Query
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::DRIVE_INFOLIST));
    const Poco::JSON::Object queryParamsObj;
    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    const Poco::JSON::Array driveInfoListObj = createDriveInfoObjList();

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("driveInfoList", driveInfoListObj);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::DRIVE_INFOLIST));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    // Job expected answer
    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto driveInfoListJob = std::dynamic_pointer_cast<DriveInfoListJob>(job);
        CPPUNIT_ASSERT(driveInfoListJob);

        driveInfoListJob->_driveInfoList = createDriveInfoList();
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = testcommhelpers::stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testDriveUpdateJob() {
    // Query
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::DRIVE_UPDATE));

    const Poco::Dynamic::Array driveInfoListObj = createDriveInfoObjList();
    const Poco::JSON::Object driveInfoObj = driveInfoListObj[0].extract<Poco::JSON::Object>();

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("driveInfo", driveInfoObj);
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
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::DRIVE_UPDATE));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);
    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        const auto driveInfoListJob = std::dynamic_pointer_cast<DriveUpdateJob>(job);
        CPPUNIT_ASSERT(driveInfoListJob);

        CPPUNIT_ASSERT(createDriveInfoList().at(0) == driveInfoListJob->_driveInfo);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testDriveDeleteJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::DRIVE_DELETE)) +
                        R"(,)"
                        R"( "params": { "driveDbId": 1 } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::DRIVE_DELETE)) +
                        R"(,)"
                        R"( "params": { "driveDbId": 1 } })"};

    // Callback expected answer
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{}})"};
#endif

    // Job expected answer
    const auto answerStr{R"({ "cause": 0,)"
                         R"( "code": 0,)"
                         R"( "id": 1,)"
                         R"( "num": )" +
                         std::to_string(toInt(RequestNum::DRIVE_DELETE)) +
                         R"(,)"
                         R"( "params": {  },)"
                         R"( "type": )" +
                         std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob>) {
        // No output parameters
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testDriveSearchJob() {
    // Base64 conversions
    // "info*" <=> "aW5mbyo="
    // "1000" <=> "MTAwMA=="
    // "2000" <=> "MjAwMA=="
    // "toto" <=> "dG90bw=="
    // "titi" <=> "dGl0aQ=="

    // Query
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::DRIVE_SEARCH));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("syncDbId", 1);
    (void) queryParamsObj.set("searchString", "aW5mbyo=");

    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("hasMore", false);
    Poco::JSON::Array searchInfoListObj;
    Poco::JSON::Object searchInfoObj1;
    (void) searchInfoObj1.set("id", "MTAwMA==");
    (void) searchInfoObj1.set("isAvailableLocally", true);
    (void) searchInfoObj1.set("modifiedTime", 10);
    (void) searchInfoObj1.set("name", "dG90bw==");
    (void) searchInfoObj1.set("path", "dG90bw==");
    (void) searchInfoObj1.set("size", 10);
    (void) searchInfoObj1.set("type", 1);
    Poco::JSON::Object searchInfoObj2;
    (void) searchInfoObj2.set("id", "MjAwMA==");
    (void) searchInfoObj2.set("isAvailableLocally", false);
    (void) searchInfoObj2.set("modifiedTime", 100);
    (void) searchInfoObj2.set("name", "dGl0aQ==");
    (void) searchInfoObj2.set("path", "dGl0aQ==");
    (void) searchInfoObj2.set("size", 100);
    (void) searchInfoObj2.set("type", 2);
    (void) searchInfoListObj.add(searchInfoObj1);
    (void) searchInfoListObj.add(searchInfoObj2);
    (void) paramsObj.set("searchInfoList", searchInfoListObj);
    (void) answerObj.set("params", paramsObj);


    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::DRIVE_SEARCH));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto driveSearchJob = std::dynamic_pointer_cast<DriveSearchJob>(job);
        CPPUNIT_ASSERT(driveSearchJob);


        const SearchInfo si1("1000", Str("toto"), NodeType::File, Str("toto"), 10, 10, true);
        const SearchInfo si2("2000", Str("titi"), NodeType::Directory, Str("titi"), 100, 100, false);

        driveSearchJob->_searchInfoList = {si1, si2};
        driveSearchJob->_hasMore = false;
    };


#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

} // namespace KDC
