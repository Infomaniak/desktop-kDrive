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

#include "comm/guijobs/blacklistednodelistjob.h"
#include "comm/guijobs/blacklistednodesetlistjob.h"
#include "comm/guijobs/nodeinfojob.h"
#include "comm/guijobs/nodesubfoldersjob.h"
#include "comm/guijobs/nodesubfolders2job.h"
#include "comm/guijobs/nodefoldersizejob.h"
#include "comm/guijobs/nodecreatemissingfoldersjob.h"

namespace KDC {

namespace {
std::string toQuotedBase64(const std::string &input) {
    std::string output;
    CommonUtility::convertToBase64Str(input, output);
    return R"(")" + output + R"(")";
}
} // namespace

void TestGuiCommChannel::testBlacklistedSyncNodeListJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1, "num": )" + std::to_string(toInt(RequestNum::BLACKLISTED_NODE_LIST)) +
                        R"(, "params": { "syncDbId": 5 } })"};
#else
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::BLACKLISTED_NODE_LIST)) +
                        R"(, "params": { "syncDbId": 5 } })"};
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{"nodeIdList":["n1","n2","n3"]}})"};
#endif
    const auto answerStr{R"({ "cause": 0, "code": 0, "id": 1, "num": )" +
                         std::to_string(toInt(RequestNum::BLACKLISTED_NODE_LIST)) +
                         R"(, "params": { "nodeIdList": [ "n1", "n2", "n3" ] }, "type": )" +
                         std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto listJob = std::dynamic_pointer_cast<BlacklistedNodeListJob>(job);
        listJob->_nodeIdList = {"n1", "n2", "n3"};
    };
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testBlacklistedSyncNodeSetListJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1, "num": )" + std::to_string(toInt(RequestNum::BLACKLISTED_NODE_SETLIST)) +
                        R"(, "params": { "syncDbId": 5, "nodeIdList": ["x1","x2"] } })"};
#else
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::BLACKLISTED_NODE_SETLIST)) +
                        R"(, "params": { "syncDbId": 5, "nodeIdList": ["x1","x2"] } })"};
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{}})"};
#endif
    const auto answerStr{R"({ "cause": 0, "code": 0, "id": 1, "num": )" +
                         std::to_string(toInt(RequestNum::BLACKLISTED_NODE_SETLIST)) + R"(, "params": {  }, "type": )" +
                         std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        // Nothing to serialize back for set job
        auto blacklistedNodeSetListJob = std::dynamic_pointer_cast<BlacklistedNodeSetListJob>(job);
        CPPUNIT_ASSERT(std::dynamic_pointer_cast<BlacklistedNodeSetListJob>(job));
        CPPUNIT_ASSERT_EQUAL(5, blacklistedNodeSetListJob->_syncDbId);
        CPPUNIT_ASSERT_EQUAL(size_t{2}, blacklistedNodeSetListJob->_nodeIdList.size());
        CPPUNIT_ASSERT_EQUAL(std::string{"x1"}, blacklistedNodeSetListJob->_nodeIdList.at(0));
        CPPUNIT_ASSERT_EQUAL(std::string{"x2"}, blacklistedNodeSetListJob->_nodeIdList.at(0));
    };
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testNodeInfoJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1, "num": )" + std::to_string(toInt(RequestNum::NODE_INFO)) +
                        R"(, "params": { "userDbId": 1, "driveId": 1111, "nodeId": "n1", "withPath": true } })"};
#else
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::NODE_INFO)) +
                        R"(, "params": { "userDbId": 1, "driveId": 1111, "nodeId": "n1", "withPath": true } })"};
    const auto cbkAnswerStr{
            R"({"cause":0,"code":0,"id":1,"params":{"nodeInfo":{"modtime":55,"name":"FileA","nodeId":"n1","parentNodeId":"n2","path":"/root/FileA","size":123}}})"};
#endif
    const auto answerStr{
            R"({ "cause": 0, "code": 0, "id": 1, "num": )" + std::to_string(toInt(RequestNum::NODE_INFO)) +
            R"(, "params": { "nodeInfo": { "modtime": 55, "name": "FileA", "nodeId": "n1", "parentNodeId": "n2", "path": "/root/FileA", "size": 123 } }, "type": )" +
            std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto nodeInfoJob = std::dynamic_pointer_cast<NodeInfoJob>(job);
        nodeInfoJob->_nodeInfo = NodeInfo("n1", "FileA", 123, "n2", 55, "/root/FileA");
    };
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testNodeSubFolderJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    // Query: list sub folders for nodeId "123" with path
    const auto queryStr{R"({ "id": 1, "num": )" + std::to_string(toInt(RequestNum::NODE_SUBFOLDERS)) +
                        R"(, "params": { "userDbId": 1, "driveId": 1111, "nodeId": "123", "withPath": true } })"};
#else
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::NODE_SUBFOLDERS)) +
                        R"(, "params": { "userDbId": 1, "driveId": 1111, "nodeId": "123", "withPath": true } })"};
    const auto cbkAnswerStr{
            R"({"cause":0,"code":0,"id":1,"params":{"nodeSubFolderInfoList":[{"modtime":10,"name":"FolderA","nodeId":"A","parentNodeId":"123","path":"/root/FolderA","size":0},{"modtime":20,"name":"FolderB","nodeId":"B","parentNodeId":"123","path":"/root/FolderB","size":0}]}})"};
#endif
    const auto answerStr{
            R"({ "cause": 0, "code": 0, "id": 1, "num": )" + std::to_string(toInt(RequestNum::NODE_SUBFOLDERS)) +
            R"(, "params": { "nodeSubFolderInfoList": [ { "modtime": 10, "name": "FolderA", "nodeId": "A", "parentNodeId": "123", "path": "/root/FolderA", "size": 0 }, { "modtime": 20, "name": "FolderB", "nodeId": "B", "parentNodeId": "123", "path": "/root/FolderB", "size": 0 } ] }, "type": )" +
            std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto nodeSubFoldersJob = std::dynamic_pointer_cast<NodeSubFoldersJob>(job);
        NodeInfo n1("A", "FolderA", 0, "123", 10, "/root/FolderA");
        NodeInfo n2("B", "FolderB", 0, "123", 20, "/root/FolderB");
        nodeSubFoldersJob->_nodeSubFolderInfoList = {n1, n2};
    };
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testNodeSubFolders2Job() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::NODE_SUBFOLDERS2)) +
                        R"(,)"
                        R"( "params": { "driveDbId": 1, "nodeId": )" +
                        toQuotedBase64("1111") + R"(, "withPath": true } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::NODE_SUBFOLDERS2)) +
                        R"(,)"
                        R"( "params": { "driveDbId": 1, "nodeId": )" +
                        toQuotedBase64("1111") + R"(, "withPath": true } })"};

    // Callback expected answer
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{"subfoldersList":[{"modtime":0,"name":)" +
                            toQuotedBase64("name") + R"(,"nodeId":)" + toQuotedBase64("1111") + R"(,"parentNodeId":)" +
                            toQuotedBase64("0000") + R"(,"path":)" + toQuotedBase64("documents/pending") + R"(,"size":1024}]}})"};
#endif
    // Job expected answer
    const auto answerStr{R"({ "cause": 0,)"
                         R"( "code": 0,)"
                         R"( "id": 1,)"
                         R"( "num": )" +
                         std::to_string(toInt(RequestNum::NODE_SUBFOLDERS2)) +
                         R"(,)"
                         R"( "params": { "subfoldersList": [ { "modtime": 0, "name": )" +
                         toQuotedBase64("name") + R"(, "nodeId": )" + toQuotedBase64("1111") + R"(, "parentNodeId": )" +
                         toQuotedBase64("0000") + R"(, "path": )" + toQuotedBase64("documents/pending") +
                         R"(, "size": 1024 } ] },)"
                         R"( "type": )" +
                         std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto nodeSubFolders2Job = std::dynamic_pointer_cast<NodeSubFolders2Job>(job);
        CPPUNIT_ASSERT(nodeSubFolders2Job);
        CPPUNIT_ASSERT_EQUAL(1, nodeSubFolders2Job->_driveDbId);
        CPPUNIT_ASSERT_EQUAL(NodeId{"1111"}, nodeSubFolders2Job->_nodeId);
        CPPUNIT_ASSERT(nodeSubFolders2Job->_withPath);

        nodeSubFolders2Job->_subfoldersList = {};
        const NodeInfo nodeInfo(QString{"1111"}, "name", 1024, "0000", 0, "documents/pending");
        nodeSubFolders2Job->_subfoldersList = {nodeInfo};
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testNodeFolderSizeJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1, "num": )" + std::to_string(toInt(RequestNum::NODE_FOLDER_SIZE)) +
                        R"(, "params": { "userDbId": 1, "driveId": 1111, "nodeId": "123" } })"};
#else
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::NODE_FOLDER_SIZE)) +
                        R"(, "params": { "userDbId": 1, "driveId": 1111, "nodeId": "123" } })"};
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{"folderSize":987654321}})"};
#endif
    const auto answerStr{R"({ "cause": 0, "code": 0, "id": 1, "num": )" + std::to_string(toInt(RequestNum::NODE_FOLDER_SIZE)) +
                         R"(, "params": { "folderSize": 987654321 }, "type": )" +
                         std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto folderSizeJob = std::dynamic_pointer_cast<NodeFolderSizeJob>(job);
        folderSizeJob->_folderSize = 987654321;
    };
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testNodeCreateMissingFoldersJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr{R"({ "id": 1,)"
                        R"( "num": )" +
                        std::to_string(toInt(RequestNum::NODE_CREATEMISSINGFOLDERS)) +
                        R"(,)"
                        R"( "params": { "driveDbId": 1, "folderList": [ { "name": )" +
                        toQuotedBase64("some-name") + R"(, "nodeId": )" + toQuotedBase64("6666") + R"( }, { "name": )" +
                        toQuotedBase64("some-other-name") + R"(, "nodeId": )" + toQuotedBase64("7777") + R"( }  ] } })"};
#else
    // There is no need to pass a request id as the response is via a callback.
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::NODE_CREATEMISSINGFOLDERS)) +
                        R"(,)"
                        R"( "params": { "driveDbId": 1, "folderList": [ { "name": )" +
                        toQuotedBase64("some-name") + R"(, "nodeId": )" + toQuotedBase64("6666") + R"( }, { "name": )" +
                        toQuotedBase64("some-other-name") + R"(, "nodeId": )" + toQuotedBase64("7777") + R"( }  ] } })"};

    // Callback expected answer
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{"parentNodeId":)" + toQuotedBase64("1111") + R"(}})"};
#endif

    // Job expected answer
    const auto answerStr{R"({ "cause": 0,)"
                         R"( "code": 0,)"
                         R"( "id": 1,)"
                         R"( "num": )" +
                         std::to_string(toInt(RequestNum::NODE_CREATEMISSINGFOLDERS)) +
                         R"(,)"
                         R"( "params": { "parentNodeId": )" +
                         toQuotedBase64("1111") +
                         R"( },)"
                         R"( "type": )" +
                         std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto nodeCreateMissingFoldersJob = std::dynamic_pointer_cast<NodeCreateMissingFoldersJob>(job);
        CPPUNIT_ASSERT(nodeCreateMissingFoldersJob);
        CPPUNIT_ASSERT_EQUAL(1, nodeCreateMissingFoldersJob->_driveDbId);
        CPPUNIT_ASSERT(NodeId{"6666"} == nodeCreateMissingFoldersJob->_folderList.at(0).nodeId);
        CPPUNIT_ASSERT(CommString{Str("some-name")} == nodeCreateMissingFoldersJob->_folderList.at(0).name);
        CPPUNIT_ASSERT(NodeId{"7777"} == nodeCreateMissingFoldersJob->_folderList.at(1).nodeId);
        CPPUNIT_ASSERT(CommString{Str("some-other-name")} == nodeCreateMissingFoldersJob->_folderList.at(1).name);

        nodeCreateMissingFoldersJob->_parentNodeId = NodeId("1111");
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}
} // namespace KDC
