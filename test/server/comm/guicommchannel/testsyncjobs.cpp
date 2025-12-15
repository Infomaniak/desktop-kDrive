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

#include "comm/guijobs/syncinfolistjob.h"
#include "comm/guijobs/syncstatusjob.h"
#include "comm/guijobs/syncaddjob.h"
#include "comm/guijobs/syncadd2job.h"
#include "comm/guijobs/syncgetpubliclinkurljob.h"
#include "comm/guijobs/syncgetprivatelinkurljob.h"
#include "comm/guijobs/syncsetsupportsvirtualfilesjob.h"
#include "comm/guijobs/syncsetrootpinstatejob.h"

namespace KDC {

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
            std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto syncInfoListJob = std::dynamic_pointer_cast<SyncInfoListJob>(job);

        const SyncInfo si1(1, 1, "/Users/test/kDrive1", "", "", true, VirtualFileMode::Win, "");
        const SyncInfo si2(2, 1, "/Users/test/kDrive2", "folder1", "999", false, VirtualFileMode::Off,
                           "{645FF040-5081-101B-9F08-00AA002F954E}");

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
                         std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

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
            std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto syncAddJob = std::dynamic_pointer_cast<SyncAddJob>(job);

        syncAddJob->syncInfo() = SyncInfo(1, 1, "/Users/test/kDrive1", "test", "999", true, VirtualFileMode::Win,
                                          "{645FF040-5081-101B-9F08-00AA002F954E}");
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
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
            std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto syncAdd2Job = std::dynamic_pointer_cast<SyncAdd2Job>(job);

        syncAdd2Job->syncInfo() = SyncInfo(1, 1, "/Users/test/kDrive1", "test", "999", true, VirtualFileMode::Win,
                                           "{645FF040-5081-101B-9F08-00AA002F954E}");
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
            std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

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
                        R"( "params": { "driveDbId": 1, "fileId": "MTExMQ==" } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::SYNC_GETPRIVATELINKURL)) +
                        R"(,)"
                        R"( "params": { "driveDbId": 1, "fileId": "MTExMQ==" } })"};

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
            std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto syncGetPrivateLinkUrlJob = std::dynamic_pointer_cast<SyncGetPrivateLinkUrlJob>(job);
        CPPUNIT_ASSERT(syncGetPrivateLinkUrlJob);
        CPPUNIT_ASSERT_EQUAL(1, syncGetPrivateLinkUrlJob->_driveDbId);
        CPPUNIT_ASSERT(CommString{Str("1111")} == syncGetPrivateLinkUrlJob->_fileId);

        syncGetPrivateLinkUrlJob->_linkUrl = std::string{"https://kdrive.infomaniak.com/app/drive/1/redirect/1111"};
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
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
                         std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

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

void TestGuiCommChannel::testSyncSetRootPinStateJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::SYNC_SETROOTPINSTATE)) +
                        R"(,)"
                        R"( "params": { "syncDbId": 1, "state": 2 } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::SYNC_SETROOTPINSTATE)) +
                        R"(,)"
                        R"( "params": { "syncDbId": 1, "state": 2 } })"};

    // Callback expected answer
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{}})"};
#endif

    // Job expected answer
    const auto answerStr{R"({ "cause": 0,)"
                         R"( "code": 0,)"
                         R"( "id": 1,)"
                         R"( "num": )" +
                         std::to_string(toInt(RequestNum::SYNC_SETROOTPINSTATE)) +
                         R"(,)"
                         R"( "params": {  },)"
                         R"( "type": )" +
                         std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto syncSetRootPinStateJob = std::dynamic_pointer_cast<SyncSetRootPinStateJob>(job);
        CPPUNIT_ASSERT(syncSetRootPinStateJob);
        CPPUNIT_ASSERT_EQUAL(1, syncSetRootPinStateJob->_syncDbId);
        CPPUNIT_ASSERT_EQUAL(PinState::OnlineOnly, syncSetRootPinStateJob->_state);
    };
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

} // namespace KDC
