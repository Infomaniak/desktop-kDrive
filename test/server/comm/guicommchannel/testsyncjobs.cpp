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

#include "../testcommhelpers.h"
#include "comm/guijobs/signalsyncnotifymanydeletesjob.h"
#include "comm/guijobs/syncacknowledgemanydeletesjob.h"

#include "comm/guijobs/syncinfolistjob.h"
#include "comm/guijobs/syncofflinefilessizejob.h"
#include "comm/guijobs/syncstatusjob.h"
#include "comm/guijobs/syncaddjob.h"
#include "comm/guijobs/syncadd2job.h"
#include "comm/guijobs/syncgetpubliclinkurljob.h"
#include "comm/guijobs/syncgetprivatelinkurljob.h"
#include "comm/guijobs/synctriggerprogressupdatejob.h"
#include "comm/guijobs/syncsetsupportsvirtualfilesjob.h"
#include "comm/guijobmanager.h"
#include "appserver/testappserver.h"
#include "comm/testsocketcomm.h"
#include "libcommonserver/keychainmanager/keychainmanager.h"
#include "libcommonserver/keychainmanager/apitoken.h"
#include "mocks/mockkeychainstorage.h"
#include "test_utility/testhelpers.h"
#include "utility/jsonparserutility.h"

#include <chrono>
#include <fstream>
#include <thread>

namespace KDC {

using namespace testcommhelpers;

void TestGuiCommChannel::testSyncInfoListJob() {
    // Base64 conversions
    // "/Users/test/kDrive1" <=> "L1VzZXJzL3Rlc3Qva0RyaXZlMQ=="
    // "/Users/test/kDrive2" <=> "L1VzZXJzL3Rlc3Qva0RyaXZlMg=="
    // "folder1" <=> "Zm9sZGVyMQ=="
    // "999" <=> "OTk5"
    // "{645FF040-5081-101B-9F08-00AA002F954E}" <=> "ezY0NUZGMDQwLTUwODEtMTAxQi05RjA4LTAwQUEwMDJGOTU0RX0="

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::SYNC_INFOLIST)) +
                        R"(,)"
                        R"( "params": { } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::SYNC_INFOLIST)) +
                        R"(,)"
                        R"( "params": { } })"};

    // Callback expected answer
    const auto cbkAnswerStr{
            R"({"cause":0,"code":0,"id":1,"params":{"syncInfoList":[)"
            R"({"dbId":1,"driveDbId":1,"localPath":"L1VzZXJzL3Rlc3Qva0RyaXZlMQ==","navigationPaneClsid":"","supportVfs":true,"targetNodeId":"","targetPath":"","virtualFileMode":1},)"
            R"({"dbId":2,"driveDbId":1,"localPath":"L1VzZXJzL3Rlc3Qva0RyaXZlMg==","navigationPaneClsid":"ezY0NUZGMDQwLTUwODEtMTAxQi05RjA4LTAwQUEwMDJGOTU0RX0=","supportVfs":false,"targetNodeId":"OTk5","targetPath":"Zm9sZGVyMQ==","virtualFileMode":0}]}})"};
#endif

    // Job expected answer
    const auto answerStr{
            R"({ "cause": 0,)"
            R"( "code": 0,)"
            R"( "id": 1,)"
            R"( "num": )" +
            std::to_string(toInt(RequestNum::SYNC_INFOLIST)) +
            R"(,)"
            R"( "params": {)"
            R"( "syncInfoList": [)"
            R"( { "dbId": 1, "driveDbId": 1, "localPath": "L1VzZXJzL3Rlc3Qva0RyaXZlMQ==", "navigationPaneClsid": "", "supportVfs": true, "targetNodeId": "", "targetPath": "", "virtualFileMode": 1 },)"
            R"( { "dbId": 2, "driveDbId": 1, "localPath": "L1VzZXJzL3Rlc3Qva0RyaXZlMg==", "navigationPaneClsid": "ezY0NUZGMDQwLTUwODEtMTAxQi05RjA4LTAwQUEwMDJGOTU0RX0=", "supportVfs": false, "targetNodeId": "OTk5", "targetPath": "Zm9sZGVyMQ==", "virtualFileMode": 0 } ] },)"
            R"( "type": )" +
            std::to_string(toInt(GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto syncInfoListJob = std::dynamic_pointer_cast<SyncInfoListJob>(job);

        Sync si1(1, 1, "/Users/test/kDrive1", "", "");
        si1.setSupportVfs(true);
        si1.setVirtualFileMode(VirtualFileMode::Win);
        Sync si2(2, 1, "/Users/test/kDrive2", "123", "folder1", "999");
        si2.setSupportVfs(false);
        si2.setVirtualFileMode(VirtualFileMode::Off);
        si2.setNavigationPaneClsid("{645FF040-5081-101B-9F08-00AA002F954E}");

        syncInfoListJob->_syncInfoList = {si1, si2};
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testStartSyncJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::SYNC_START)) +
                        R"(,)"
                        R"( "params": { "syncDbId": 1 } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::SYNC_START)) +
                        R"(,)"
                        R"( "params": { "syncDbId": 1 } })"};

    // Callback expected answer
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{}})"};
#endif

    // Job expected answer
    const auto answerStr{R"({ "cause": 0,)"
                         R"( "code": 0,)"
                         R"( "id": 1,)"
                         R"( "num": )" +
                         std::to_string(toInt(RequestNum::SYNC_START)) +
                         R"(,)"
                         R"( "params": {  },)"
                         R"( "type": )" +
                         std::to_string(toInt(GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob>) {
        // No output parameters
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testStopSyncJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::SYNC_STOP)) +
                        R"(,)"
                        R"( "params": { "syncDbId": 1 } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::SYNC_STOP)) +
                        R"(,)"
                        R"( "params": { "syncDbId": 1 } })"};

    // Callback expected answer
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{}})"};
#endif

    // Job expected answer
    const auto answerStr{R"({ "cause": 0,)"
                         R"( "code": 0,)"
                         R"( "id": 1,)"
                         R"( "num": )" +
                         std::to_string(toInt(RequestNum::SYNC_STOP)) +
                         R"(,)"
                         R"( "params": {  },)"
                         R"( "type": )" +
                         std::to_string(toInt(GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob>) {
        // No output parameters
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testSyncStatusJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::SYNC_STATUS)) +
                        R"(,)"
                        R"( "params": { "syncDbId": 1 } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::SYNC_STATUS)) +
                        R"(,)"
                        R"( "params": { "syncDbId": 1 } })"};

    // Callback expected answer
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{"syncStatus":3}})"};
#endif

    // Job expected answer
    const auto answerStr{R"({ "cause": 0,)"
                         R"( "code": 0,)"
                         R"( "id": 1,)"
                         R"( "num": )" +
                         std::to_string(toInt(RequestNum::SYNC_STATUS)) +
                         R"(,)"
                         R"( "params": { "syncStatus": 3 },)" // SyncStatus::Idle
                         R"( "type": )" +
                         std::to_string(toInt(GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto syncStatusJob = std::dynamic_pointer_cast<SyncStatusJob>(job);

        syncStatusJob->_syncStatus = SyncStatus::Idle;
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testSyncAddJob() {
    // Base64 conversions
    // "/Users/test/kDrive1" <=> "L1VzZXJzL3Rlc3Qva0RyaXZlMQ=="
    // "test" <=> "dGVzdA=="
    // "999" <=> "OTk5"
    // "1111" <=> "MTExMQ=="
    // "2222" <=> "MjIyMg=="
    // "{645FF040-5081-101B-9F08-00AA002F954E}" <=> "ezY0NUZGMDQwLTUwODEtMTAxQi05RjA4LTAwQUEwMDJGOTU0RX0="

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::SYNC_ADD)) +
                        R"(,)"
                        R"( "params": {)"
                        R"( "userDbId": 1,)"
                        R"( "accountId": 1,)"
                        R"( "driveId": 1,)"
                        R"( "localFolderPath": "L1VzZXJzL3Rlc3Qva0RyaXZlMQ==",)"
                        R"( "serverFolderPath": "dGVzdA==",)"
                        R"( "serverFolderNodeId": "OTk5",)"
                        R"( "liteSync": 1,)"
                        R"( "blackList": [ "MTExMQ==", "MjIyMg==" ],)"
                        R"( "whiteList": [  ] } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::SYNC_ADD)) +
                        R"(,)"
                        R"( "params": {)"
                        R"( "userDbId": 1,)"
                        R"( "accountId": 1,)"
                        R"( "driveId": 1,)"
                        R"( "localFolderPath": "L1VzZXJzL3Rlc3Qva0RyaXZlMQ==",)"
                        R"( "serverFolderPath": "dGVzdA==",)"
                        R"( "serverFolderNodeId": "OTk5",)"
                        R"( "liteSync": 1,)"
                        R"( "blackList": [ "MTExMQ==", "MjIyMg==" ],)"
                        R"( "whiteList": [  ] } })"};

    // Callback expected answer
    const auto cbkAnswerStr{
            R"({"cause":0,"code":0,"id":1,"params":{"syncInfo":{"dbId":1,"driveDbId":1,"localPath":"L1VzZXJzL3Rlc3Qva0RyaXZlMQ==","navigationPaneClsid":"ezY0NUZGMDQwLTUwODEtMTAxQi05RjA4LTAwQUEwMDJGOTU0RX0=","supportVfs":true,"targetNodeId":"OTk5","targetPath":"dGVzdA==","virtualFileMode":1}}})"};
#endif

    // Job expected answer
    const auto answerStr{
            R"({ "cause": 0,)"
            R"( "code": 0,)"
            R"( "id": 1,)"
            R"( "num": )" +
            std::to_string(toInt(RequestNum::SYNC_ADD)) +
            R"(,)"
            R"( "params": { "syncInfo": { "dbId": 1, "driveDbId": 1, "localPath": "L1VzZXJzL3Rlc3Qva0RyaXZlMQ==", "navigationPaneClsid": "ezY0NUZGMDQwLTUwODEtMTAxQi05RjA4LTAwQUEwMDJGOTU0RX0=", "supportVfs": true, "targetNodeId": "OTk5", "targetPath": "dGVzdA==", "virtualFileMode": 1 } },)"
            R"( "type": )" +
            std::to_string(toInt(GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto syncAddJob = std::dynamic_pointer_cast<SyncAddJob>(job);

        syncAddJob->sync() = Sync(1, 1, "/Users/test/kDrive1", "", "test", "999", false, true, VirtualFileMode::Win, false, "",
                                  false, "{645FF040-5081-101B-9F08-00AA002F954E}");
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testSyncAddJobPartialFailureSignals() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    if (!testhelpers::isRunningOnCI()) {
        CPPUNIT_SKIP();
    }

    const testhelpers::TestVariables testVariables;

    auto keyChainManager = KeyChainManager::instance(std::make_shared<MockKeyChainStorage>());
    CPPUNIT_ASSERT(keyChainManager);

    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);
    apiToken.setUserId(std::stoi(testVariables.userId));

    const std::string keychainKey = "testSyncAddJobPartialFailureSignals";
    CPPUNIT_ASSERT(keyChainManager->writeData(keychainKey, apiToken.reconstructJsonString()));

    const User user(1, std::stoi(testVariables.userId), keychainKey);
    CPPUNIT_ASSERT(ParmsDb::instance()->insertUser(user));

    SyncPath blockedPath = _localTempDir.path() / "sync_add_job_blocker";
    {
        std::ofstream blockedPathStream(blockedPath);
        CPPUNIT_ASSERT(blockedPathStream.good());
    }

    std::string localFolderPath64Str;
    CommonUtility::convertToBase64Str(Path2Str(blockedPath), localFolderPath64Str);

    const std::string queryStr = R"({ "id": 1, "num": )" + std::to_string(toInt(RequestNum::SYNC_ADD)) +
                                 R"(, "params": { "userDbId": 1, "accountId": )" + testVariables.accountId + R"(, "driveId": )" +
                                 testVariables.driveId + R"(, "localFolderPath": ")" + localFolderPath64Str +
                                 R"(", "serverFolderPath": "dGVzdA==", "serverFolderNodeId": "OTk5", "liteSync": 1, )"
                                 R"("blackList": [  ], "whiteList": [  ] } })";

    int requestId = 0;
    RequestNum requestNum = RequestNum::Unknown;
    Poco::DynamicStruct inParams;
    CPPUNIT_ASSERT(AbstractGuiJob::deserializeGenericInputParms(CommonUtility::str2CommString(queryStr), requestId, requestNum,
                                                                inParams));

    std::unique_ptr<MockAppServer> ownedAppServer;
    auto *appServer = dynamic_cast<AppServer *>(QCoreApplication::instance());
    if (!appServer) {
        const std::vector<std::string> args = {Path2Str(CommonUtility::applicationFilePath())};
        std::vector<char *> argv;
        argv.reserve(args.size());
        for (const auto &arg: args) {
            argv.push_back(const_cast<char *>(arg.c_str()));
        }
        auto argc = static_cast<int>(argv.size());
        ownedAppServer = std::make_unique<MockAppServer>(argc, argv.data());
        appServer = ownedAppServer.get();
    }

    GuiJobManagerSingleton::clear();

    auto commManager = std::make_shared<CommManager>(*appServer);
    commManager->start();

    auto clientSocket = TestSocketComm::newSecureClient(commManager->tryGetGUICommPort());
    auto clientChannel = std::make_shared<GuiCommChannel>(clientSocket);

    auto remainingConnectionWait = 100;
    while (!commManager->hasActiveGuiConnection() && remainingConnectionWait-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CPPUNIT_ASSERT(commManager->hasActiveGuiConnection());

    auto syncAddJob = std::make_shared<SyncAddJob>(commManager, requestId, inParams, std::make_shared<GuiCommChannelTest>());
    CPPUNIT_ASSERT(syncAddJob->deserializeInputParms());

    const auto exitInfo = syncAddJob->process();
    CPPUNIT_ASSERT(!exitInfo);

    std::vector<SignalNum> signalNums;
    auto remainingMessageWait = 100;
    while (signalNums.size() < 2 && remainingMessageWait-- > 0) {
        if (clientChannel->canReadMessage()) {
            Poco::JSON::Parser parser;
            auto signalMessage = parser.parse(CommonUtility::commString2Str(clientChannel->readMessage()));
            auto signalStruct = signalMessage.extract<Poco::DynamicStruct>();

            SignalNum signalNum = SignalNum::Unknown;
            CommonUtility::readValueFromStruct(signalStruct, "num", signalNum);
            signalNums.push_back(signalNum);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    while (clientChannel->canReadMessage()) {
        Poco::JSON::Parser parser;
        auto signalMessage = parser.parse(CommonUtility::commString2Str(clientChannel->readMessage()));
        auto signalStruct = signalMessage.extract<Poco::DynamicStruct>();

        SignalNum signalNum = SignalNum::Unknown;
        CommonUtility::readValueFromStruct(signalStruct, "num", signalNum);
        signalNums.push_back(signalNum);
    }

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), signalNums.size());
    CPPUNIT_ASSERT_EQUAL(SignalNum::ACCOUNT_ADDED, signalNums[0]);
    CPPUNIT_ASSERT_EQUAL(SignalNum::DRIVE_ADDED, signalNums[1]);

    clientChannel->close();
    commManager->stop();
    GuiJobManagerSingleton::clear();
#else
    CPPUNIT_SKIP();
#endif
}

void TestGuiCommChannel::testSyncAdd2Job() {
    // Base64 conversions
    // "/Users/test/kDrive1" <=> "L1VzZXJzL3Rlc3Qva0RyaXZlMQ=="
    // "test" <=> "dGVzdA=="
    // "999" <=> "OTk5"
    // "1111" <=> "MTExMQ=="
    // "2222" <=> "MjIyMg=="
    // "{645FF040-5081-101B-9F08-00AA002F954E}" <=> "ezY0NUZGMDQwLTUwODEtMTAxQi05RjA4LTAwQUEwMDJGOTU0RX0="

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::SYNC_ADD2)) +
                        R"(,)"
                        R"( "params": {)"
                        R"( "driveDbId": 1,)"
                        R"( "localFolderPath": "L1VzZXJzL3Rlc3Qva0RyaXZlMQ==",)"
                        R"( "serverFolderPath": "dGVzdA==",)"
                        R"( "serverFolderNodeId": "OTk5",)"
                        R"( "liteSync": 1,)"
                        R"( "blackList": [ "MTExMQ==", "MjIyMg==" ],)"
                        R"( "whiteList": [  ] } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::SYNC_ADD2)) +
                        R"(,)"
                        R"( "params": {)"
                        R"( "driveDbId": 1,)"
                        R"( "localFolderPath": "L1VzZXJzL3Rlc3Qva0RyaXZlMQ==",)"
                        R"( "serverFolderPath": "dGVzdA==",)"
                        R"( "serverFolderNodeId": "OTk5",)"
                        R"( "liteSync": 1,)"
                        R"( "blackList": [ "MTExMQ==", "MjIyMg==" ],)"
                        R"( "whiteList": [  ] } })"};

    // Callback expected answer
    const auto cbkAnswerStr{
            R"({"cause":0,"code":0,"id":1,"params":{"syncInfo":{"dbId":1,"driveDbId":1,"localPath":"L1VzZXJzL3Rlc3Qva0RyaXZlMQ==","navigationPaneClsid":"ezY0NUZGMDQwLTUwODEtMTAxQi05RjA4LTAwQUEwMDJGOTU0RX0=","supportVfs":true,"targetNodeId":"OTk5","targetPath":"dGVzdA==","virtualFileMode":1}}})"};
#endif

    // Job expected answer
    const auto answerStr{
            R"({ "cause": 0,)"
            R"( "code": 0,)"
            R"( "id": 1,)"
            R"( "num": )" +
            std::to_string(toInt(RequestNum::SYNC_ADD2)) +
            R"(,)"
            R"( "params": { "syncInfo": { "dbId": 1, "driveDbId": 1, "localPath": "L1VzZXJzL3Rlc3Qva0RyaXZlMQ==", "navigationPaneClsid": "ezY0NUZGMDQwLTUwODEtMTAxQi05RjA4LTAwQUEwMDJGOTU0RX0=", "supportVfs": true, "targetNodeId": "OTk5", "targetPath": "dGVzdA==", "virtualFileMode": 1 } },)"
            R"( "type": )" +
            std::to_string(toInt(GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto syncAdd2Job = std::dynamic_pointer_cast<SyncAdd2Job>(job);

        syncAdd2Job->sync() = Sync(1, 1, "/Users/test/kDrive1", "", "test", "999", false, true, VirtualFileMode::Win, false, "",
                                   false, "{645FF040-5081-101B-9F08-00AA002F954E}");
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testSyncStartAfterLoginJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::SYNC_START_AFTER_LOGIN)) +
                        R"(,)"
                        R"( "params": { "userDbId": 1 } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::SYNC_START_AFTER_LOGIN)) +
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
                         std::to_string(toInt(RequestNum::SYNC_START_AFTER_LOGIN)) +
                         R"(,)"
                         R"( "params": {  },)"
                         R"( "type": )" +
                         std::to_string(toInt(GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob>) {
        // No output parameters
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testSyncDeleteJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::SYNC_DELETE)) +
                        R"(,)"
                        R"( "params": { "syncDbId": 1 } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::SYNC_DELETE)) +
                        R"(,)"
                        R"( "params": { "syncDbId": 1 } })"};

    // Callback expected answer
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{}})"};
#endif

    // Job expected answer
    const auto answerStr{R"({ "cause": 0,)"
                         R"( "code": 0,)"
                         R"( "id": 1,)"
                         R"( "num": )" +
                         std::to_string(toInt(RequestNum::SYNC_DELETE)) +
                         R"(,)"
                         R"( "params": {  },)"
                         R"( "type": )" +
                         std::to_string(toInt(GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob>) {
        // No output parameters
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testSyncGetPublicLinkUrlJob() {
    // Base64 conversions
    // "1111" <=> "MTExMQ=="
    // "https://kdrive.infomaniak.com/app/share/012345/abcdef" <=>
    // "aHR0cHM6Ly9rZHJpdmUuaW5mb21hbmlhay5jb20vYXBwL3NoYXJlLzAxMjM0NS9hYmNkZWY="

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::SYNC_GETPUBLICLINKURL)) +
                        R"(,)"
                        R"( "params": { "driveDbId": 1, "nodeId": "MTExMQ==" } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::SYNC_GETPUBLICLINKURL)) +
                        R"(,)"
                        R"( "params": { "driveDbId": 1, "nodeId": "MTExMQ==" } })"};

    // Callback expected answer
    const auto cbkAnswerStr{
            R"({"cause":0,"code":0,"id":1,"params":{"linkUrl":"aHR0cHM6Ly9rZHJpdmUuaW5mb21hbmlhay5jb20vYXBwL3NoYXJlLzAxMjM0NS9hYmNkZWY="}})"};
#endif

    // Job expected answer
    const auto answerStr{
            R"({ "cause": 0,)"
            R"( "code": 0,)"
            R"( "id": 1,)"
            R"( "num": )" +
            std::to_string(toInt(RequestNum::SYNC_GETPUBLICLINKURL)) +
            R"(,)"
            R"( "params": { "linkUrl": "aHR0cHM6Ly9rZHJpdmUuaW5mb21hbmlhay5jb20vYXBwL3NoYXJlLzAxMjM0NS9hYmNkZWY=" },)"
            R"( "type": )" +
            std::to_string(toInt(GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto syncGetPublicLinkUrlJob = std::dynamic_pointer_cast<SyncGetPublicLinkUrlJob>(job);

        syncGetPublicLinkUrlJob->_linkUrl = "https://kdrive.infomaniak.com/app/share/012345/abcdef";
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testSyncGetPrivateLinkUrlJob() {
    // Base64 conversions
    // "1111" <=> "MTExMQ=="
    // "https://kdrive.infomaniak.com/app/drive/1/redirect/1111" <=>
    // "aHR0cHM6Ly9rZHJpdmUuaW5mb21hbmlhay5jb20vYXBwL2RyaXZlLzEvcmVkaXJlY3QvMTExMQ=="

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::SYNC_GETPRIVATELINKURL)) +
                        R"(,)"
                        R"( "params": { "driveDbId": 1, "nodeId": "MTExMQ==" } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::SYNC_GETPRIVATELINKURL)) +
                        R"(,)"
                        R"( "params": { "driveDbId": 1, "nodeId": "MTExMQ==" } })"};

    // Callback expected answer
    const auto cbkAnswerStr{
            R"({"cause":0,"code":0,"id":1,"params":{"linkUrl":"aHR0cHM6Ly9rZHJpdmUuaW5mb21hbmlhay5jb20vYXBwL2RyaXZlLzEvcmVkaXJlY3QvMTExMQ=="}})"};
#endif

    // Job expected answer
    const auto answerStr{
            R"({ "cause": 0,)"
            R"( "code": 0,)"
            R"( "id": 1,)"
            R"( "num": )" +
            std::to_string(toInt(RequestNum::SYNC_GETPRIVATELINKURL)) +
            R"(,)"
            R"( "params": { "linkUrl": "aHR0cHM6Ly9rZHJpdmUuaW5mb21hbmlhay5jb20vYXBwL2RyaXZlLzEvcmVkaXJlY3QvMTExMQ==" },)"
            R"( "type": )" +
            std::to_string(toInt(GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto syncGetPrivateLinkUrlJob = std::dynamic_pointer_cast<SyncGetPrivateLinkUrlJob>(job);
        CPPUNIT_ASSERT(syncGetPrivateLinkUrlJob);
        CPPUNIT_ASSERT_EQUAL(1, syncGetPrivateLinkUrlJob->_driveDbId);
        CPPUNIT_ASSERT(CommString{Str("1111")} == syncGetPrivateLinkUrlJob->_nodeId);

        syncGetPrivateLinkUrlJob->_linkUrl = std::string{"https://kdrive.infomaniak.com/app/drive/1/redirect/1111"};
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testSyncTriggerProgressUpdateJob() {
    const Poco::JSON::Object query = createSimpleQuery(RequestNum::SYNC_TRIGGER_PROGRESS_UPDATE);
    const auto queryStr = stringifyQueryObj(query);

    // Job expected answers
    const SimpleAnswers simpleAnswers = createSimpleAnswers(RequestNum::SYNC_TRIGGER_PROGRESS_UPDATE);
    const auto answerStr = stringifyAnswerObj(simpleAnswers.answerWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        const auto syncTriggerProgressUpdateJob = std::dynamic_pointer_cast<SyncTriggerProgressUpdateJob>(job);
        CPPUNIT_ASSERT(syncTriggerProgressUpdateJob);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(simpleAnswers.answer);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}


void TestGuiCommChannel::testSyncSetSupportsVirtualFilesJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::SYNC_SETSUPPORTSVIRTUALFILES)) +
                        R"(,)"
                        R"( "params": { "syncDbId": 1, "value": false } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::SYNC_SETSUPPORTSVIRTUALFILES)) +
                        R"(,)"
                        R"( "params": { "syncDbId": 1, "value": false } })"};

    // Callback expected answer
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{}})"};
#endif

    // Job expected answer
    const auto answerStr{R"({ "cause": 0,)"
                         R"( "code": 0,)"
                         R"( "id": 1,)"
                         R"( "num": )" +
                         std::to_string(toInt(RequestNum::SYNC_SETSUPPORTSVIRTUALFILES)) +
                         R"(,)"
                         R"( "params": {  },)"
                         R"( "type": )" +
                         std::to_string(toInt(GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto syncSetSupportsVirtualFilesJob = std::dynamic_pointer_cast<SyncSetSupportsVirtualFilesJob>(job);
        CPPUNIT_ASSERT(syncSetSupportsVirtualFilesJob);
        CPPUNIT_ASSERT_EQUAL(1, syncSetSupportsVirtualFilesJob->_syncDbId);
        CPPUNIT_ASSERT(!syncSetSupportsVirtualFilesJob->_value);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testSyncOfflineFilesSizeJob() {
    // Query
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::SYNC_OFFLINE_FILES_SIZE));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("syncDbId", 1);
    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("size", 10);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::SYNC_OFFLINE_FILES_SIZE));
    (void) answerObjWithNumAndType.set("type", toInt(GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto syncOfflineFilesSizeJob = std::dynamic_pointer_cast<SyncOfflineFilesSizeJob>(job);
        CPPUNIT_ASSERT(syncOfflineFilesSizeJob);
        syncOfflineFilesSizeJob->_size = 10;
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testSignalSyncNotifyManyDeletes() {
    const SyncDbId syncDbId = 1;
    const TooManyDeletesNotificationType notificationType = TooManyDeletesNotificationType::HardLimit;
    const std::vector<SyncPath> filesPaths = {"file1", "file2", "file3"};
    const int32_t nbFiles = static_cast<int32_t>(10);
    SignalSyncNotifyManyDeletesJob job(syncDbId, notificationType, nbFiles, filesPaths);

    checkSignalCommonMethods(job, SignalNum::SYNC_NOTIFY_MANY_DELETES);

    if (!job.serializeGenericOutputParms(ExitCode::Ok)) {
        CPPUNIT_ASSERT(false);
    }

    const auto jsonObj =
            Poco::JSON::Parser{}.parse(CommonUtility::commString2Str(job._outputParamsStr)).extract<Poco::JSON::Object::Ptr>();
    const auto paramsObj = JsonParserUtility::extractJsonObject(jsonObj, "params");
    SyncDbId syncDbIdOut = 0;
    (void) JsonParserUtility::extractValue(paramsObj, "syncDbId", syncDbIdOut);
    CPPUNIT_ASSERT_EQUAL(static_cast<SyncDbId>(1), syncDbIdOut);
    auto notificationTypeOut = 0;
    (void) JsonParserUtility::extractValue(paramsObj, "notificationType", notificationTypeOut);
    CPPUNIT_ASSERT_EQUAL(TooManyDeletesNotificationType::HardLimit,
                         static_cast<TooManyDeletesNotificationType>(notificationTypeOut));
    auto nbFilesOut = 0;
    (void) JsonParserUtility::extractValue(paramsObj, "nbFiles", nbFilesOut);
    CPPUNIT_ASSERT_EQUAL(nbFiles, nbFilesOut);
    std::vector<CommString> filesPathsOut;
    const auto filesPathsArray = JsonParserUtility::extractArrayObject(paramsObj, "filesPaths");
    CPPUNIT_ASSERT(filesPathsArray);
    for (const auto &pathVar: *filesPathsArray) {
        filesPathsOut.push_back(CommonUtility::commString2SyncPath(pathVar.convert<CommString>()));
    }
    CPPUNIT_ASSERT_EQUAL(filesPaths.size(), filesPathsOut.size());
    for (size_t i = 0; i < filesPaths.size(); ++i) {
        SyncName decoded;
        CommonUtility::convertFromBase64Str(CommonUtility::commString2Str(filesPathsOut[i]), decoded);
        CPPUNIT_ASSERT(filesPaths[i].native() == decoded);
    }
}

void TestGuiCommChannel::testAcknowledgeManyDeletes() {
    // Query
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::SYNC_ACKNOWLEDGE_MANY_DELETES));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("syncDbId", 1);
    (void) queryParamsObj.set("userChoice", toInt(TooManyDeletesUserChoice::Revert));
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
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::SYNC_ACKNOWLEDGE_MANY_DELETES));
    (void) answerObjWithNumAndType.set("type", toInt(GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto syncAcknowledgeManyDeletesJob = std::dynamic_pointer_cast<SyncAcknowledgeManyDeletesJob>(job);
        CPPUNIT_ASSERT(syncAcknowledgeManyDeletesJob);
        CPPUNIT_ASSERT_EQUAL(static_cast<SyncDbId>(1), syncAcknowledgeManyDeletesJob->_syncDbId);
        CPPUNIT_ASSERT_EQUAL(TooManyDeletesUserChoice::Revert, syncAcknowledgeManyDeletesJob->_userChoice);
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

} // namespace KDC
