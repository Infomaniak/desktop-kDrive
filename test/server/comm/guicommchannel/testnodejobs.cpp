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

#include "comm/guijobs/blacklistednodelistjob.h"
#include "comm/guijobs/blacklistednodesetlistjob.h"
#include "comm/guijobs/nodepathjob.h"
#include "comm/guijobs/nodeinfojob.h"
#include "comm/guijobs/nodesubfoldersjob.h"
#include "comm/guijobs/nodesubfolders2job.h"
#include "comm/guijobs/nodefoldersizejob.h"
#include "comm/guijobs/nodecreatemissingfoldersjob.h"

namespace KDC {
using namespace testcommhelpers;

void TestGuiCommChannel::testBlacklistedSyncNodeListJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::BLACKLISTED_NODE_LIST));
    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("syncDbId", 5);

    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Array nodeIdListObj;
    (void) nodeIdListObj.add(toBase64(Str("n1")));
    (void) nodeIdListObj.add(toBase64(Str("n2")));
    (void) nodeIdListObj.add(toBase64(Str("n3")));

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("nodeIdList", nodeIdListObj);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::BLACKLISTED_NODE_LIST));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answer
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto listJob = std::dynamic_pointer_cast<BlacklistedNodeListJob>(job);
        listJob->_nodeIdList = {"n1", "n2", "n3"};
    };
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testBlacklistedSyncNodeSetListJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::BLACKLISTED_NODE_SETLIST));

    Poco::JSON::Array nodeIdListObj;
    (void) nodeIdListObj.add(toBase64(Str("x1")));
    (void) nodeIdListObj.add(toBase64(Str("x2")));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("syncDbId", 5);
    (void) queryParamsObj.set("nodeIdList", nodeIdListObj);

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
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::BLACKLISTED_NODE_SETLIST));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        // Nothing to serialize back for set job
        auto blacklistedNodeSetListJob = std::dynamic_pointer_cast<BlacklistedNodeSetListJob>(job);
        CPPUNIT_ASSERT(blacklistedNodeSetListJob);
        CPPUNIT_ASSERT_EQUAL(5, blacklistedNodeSetListJob->_syncDbId);
        CPPUNIT_ASSERT_EQUAL(size_t{2}, blacklistedNodeSetListJob->_nodeIdList.size());
        CPPUNIT_ASSERT_EQUAL(std::string{"x1"}, blacklistedNodeSetListJob->_nodeIdList.at(0));
        CPPUNIT_ASSERT_EQUAL(std::string{"x2"}, blacklistedNodeSetListJob->_nodeIdList.at(1));
    };
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testNodePathJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::NODE_PATH));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("syncDbId", 1);
    (void) queryParamsObj.set("nodeId", toBase64(Str("1111")));

    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("path", toBase64(Str("/home/kDrive/Documents/presentation.doc")));
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::NODE_PATH));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto nodePathJob = std::dynamic_pointer_cast<NodePathJob>(job);
        CPPUNIT_ASSERT(nodePathJob);
        CPPUNIT_ASSERT_EQUAL(1, nodePathJob->_syncDbId);
        CPPUNIT_ASSERT_EQUAL(NodeId{"1111"}, nodePathJob->_nodeId);

        nodePathJob->_path = CommString{Str("/home/kDrive/Documents/presentation.doc")};
    };
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testNodeInfoJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::NODE_INFO));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("userDbId", 1);
    (void) queryParamsObj.set("driveId", 1111);
    (void) queryParamsObj.set("nodeId", toBase64(Str("n1")));
    (void) queryParamsObj.set("withPath", true);

    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object nodeInfoObj;
    (void) nodeInfoObj.set("modtime", 55);
    (void) nodeInfoObj.set("name", toBase64(Str("FileA")));
    (void) nodeInfoObj.set("nodeId", toBase64(Str("n1")));
    (void) nodeInfoObj.set("parentNodeId", toBase64(Str("n2")));
    (void) nodeInfoObj.set("path", toBase64(Str("/root/FileA")));
    (void) nodeInfoObj.set("size", 123);
    (void) nodeInfoObj.set("accessDenied", false);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("nodeInfo", nodeInfoObj);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::NODE_INFO));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto nodeInfoJob = std::dynamic_pointer_cast<NodeInfoJob>(job);
        CPPUNIT_ASSERT_EQUAL(1, nodeInfoJob->_userDbId);
        CPPUNIT_ASSERT_EQUAL(1111, nodeInfoJob->_driveId);
        CPPUNIT_ASSERT_EQUAL(NodeId{"n1"}, nodeInfoJob->_nodeId);
        CPPUNIT_ASSERT(nodeInfoJob->_withPath);

        nodeInfoJob->_nodeInfo = NodeInfo("n1", "FileA", 123, "n2", 55, "/root/FileA");
        nodeInfoJob->_nodeInfo.setAccessDenied(false);
    };
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testNodeSubFolderJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::NODE_SUBFOLDERS));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("userDbId", 1);
    (void) queryParamsObj.set("driveId", 1111);
    (void) queryParamsObj.set("nodeId", toBase64(Str("123")));
    (void) queryParamsObj.set("withPath", true);

    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object nodeInfoObj1;
    (void) nodeInfoObj1.set("modtime", 10);
    (void) nodeInfoObj1.set("name", toBase64(Str("FolderA")));
    (void) nodeInfoObj1.set("nodeId", toBase64(Str("A")));
    (void) nodeInfoObj1.set("parentNodeId", toBase64(Str("123")));
    (void) nodeInfoObj1.set("path", toBase64(Str("/root/FolderA")));
    (void) nodeInfoObj1.set("size", 0);
    (void) nodeInfoObj1.set("accessDenied", false);

    Poco::JSON::Object nodeInfoObj2;
    (void) nodeInfoObj2.set("modtime", 20);
    (void) nodeInfoObj2.set("name", toBase64(Str("FolderB")));
    (void) nodeInfoObj2.set("nodeId", toBase64(Str("B")));
    (void) nodeInfoObj2.set("parentNodeId", toBase64(Str("123")));
    (void) nodeInfoObj2.set("path", toBase64(Str("/root/FolderB")));
    (void) nodeInfoObj2.set("size", 0);
    (void) nodeInfoObj2.set("accessDenied", false);

    Poco::JSON::Array nodeSubFolderInfoListObj;
    (void) nodeSubFolderInfoListObj.add(nodeInfoObj1);
    (void) nodeSubFolderInfoListObj.add(nodeInfoObj2);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("nodeSubFolderInfoList", nodeSubFolderInfoListObj);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::NODE_SUBFOLDERS));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto nodeSubFoldersJob = std::dynamic_pointer_cast<NodeSubFoldersJob>(job);
        CPPUNIT_ASSERT(nodeSubFoldersJob);
        CPPUNIT_ASSERT_EQUAL(1, nodeSubFoldersJob->_userDbId);
        CPPUNIT_ASSERT_EQUAL(1111, nodeSubFoldersJob->_driveId);
        CPPUNIT_ASSERT_EQUAL(NodeId{"123"}, nodeSubFoldersJob->_nodeId);
        CPPUNIT_ASSERT(nodeSubFoldersJob->_withPath);

        const NodeInfo n1("A", "FolderA", 0, "123", 10, "/root/FolderA");
        const NodeInfo n2("B", "FolderB", 0, "123", 20, "/root/FolderB");
        nodeSubFoldersJob->_nodeSubFolderInfoList = {n1, n2};
    };
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testNodeSubFolders2Job() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::NODE_SUBFOLDERS2));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("driveDbId", 1);
    (void) queryParamsObj.set("nodeId", toBase64(Str("someNodeId")));
    (void) queryParamsObj.set("withPath", true);

    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object nodeInfoObj1;
    (void) nodeInfoObj1.set("modtime", 0);
    (void) nodeInfoObj1.set("name", toBase64(Str("name1")));
    (void) nodeInfoObj1.set("nodeId", toBase64(Str("1111")));
    (void) nodeInfoObj1.set("parentNodeId", toBase64(Str("0000")));
    (void) nodeInfoObj1.set("path", toBase64(Str("documents/pending1")));
    (void) nodeInfoObj1.set("size", 1024);
    (void) nodeInfoObj1.set("accessDenied", false);

    Poco::JSON::Object nodeInfoObj2;
    (void) nodeInfoObj2.set("modtime", 0);
    (void) nodeInfoObj2.set("name", toBase64(Str("name2")));
    (void) nodeInfoObj2.set("nodeId", toBase64(Str("2222")));
    (void) nodeInfoObj2.set("parentNodeId", toBase64(Str("0000")));
    (void) nodeInfoObj2.set("path", toBase64(Str("documents/pending2")));
    (void) nodeInfoObj2.set("size", 1024);
    (void) nodeInfoObj2.set("accessDenied", false);

    Poco::JSON::Array nodeSubFolderInfoListObj;
    (void) nodeSubFolderInfoListObj.add(nodeInfoObj1);
    (void) nodeSubFolderInfoListObj.add(nodeInfoObj2);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("nodeSubFolderInfoList", nodeSubFolderInfoListObj);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::NODE_SUBFOLDERS2));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answers
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto nodeSubFolders2Job = std::dynamic_pointer_cast<NodeSubFolders2Job>(job);
        CPPUNIT_ASSERT(nodeSubFolders2Job);
        CPPUNIT_ASSERT_EQUAL(1, nodeSubFolders2Job->_driveDbId);
        CPPUNIT_ASSERT_EQUAL(NodeId{"someNodeId"}, nodeSubFolders2Job->_nodeId);
        CPPUNIT_ASSERT(nodeSubFolders2Job->_withPath);

        const NodeInfo nodeInfo1(QString{"1111"}, "name1", 1024, "0000", 0, "documents/pending1");
        const NodeInfo nodeInfo2(QString{"2222"}, "name2", 1024, "0000", 0, "documents/pending2");

        nodeSubFolders2Job->_nodeSubFolderInfoList = {nodeInfo1, nodeInfo2};
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testNodeFolderSizeJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::NODE_FOLDER_SIZE));

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("userDbId", 1);
    (void) queryParamsObj.set("driveId", 1111);
    (void) queryParamsObj.set("nodeId", toBase64(Str("123")));

    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("folderSize", 987654321);
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::NODE_FOLDER_SIZE));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto folderSizeJob = std::dynamic_pointer_cast<NodeFolderSizeJob>(job);
        CPPUNIT_ASSERT_EQUAL(1, folderSizeJob->_userDbId);
        CPPUNIT_ASSERT_EQUAL(1111, folderSizeJob->_driveId);
        CPPUNIT_ASSERT_EQUAL(NodeId{"123"}, folderSizeJob->_nodeId);

        folderSizeJob->_folderSize = 987654321;
    };
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}

void TestGuiCommChannel::testNodeCreateMissingFoldersJob() {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(RequestNum::NODE_CREATEMISSINGFOLDERS));

    Poco::JSON::Object folderObj1;
    (void) folderObj1.set("name", toBase64(Str("someName")));
    (void) folderObj1.set("nodeId", toBase64(Str("6666")));

    Poco::JSON::Object folderObj2;
    (void) folderObj2.set("name", toBase64(Str("someOtherName")));
    (void) folderObj2.set("nodeId", toBase64(Str("7777")));

    Poco::JSON::Array folderListObj;
    (void) folderListObj.add(folderObj1);
    (void) folderListObj.add(folderObj2);

    Poco::JSON::Object queryParamsObj;
    (void) queryParamsObj.set("driveDbId", 1);
    (void) queryParamsObj.set("folderList", folderListObj);

    (void) queryObj.set("params", queryParamsObj);
    const auto queryStr = stringifyQueryObj(queryObj);

    // Answer
    Poco::JSON::Object answerObj;
    (void) answerObj.set("cause", 0);
    (void) answerObj.set("code", 0);
    (void) answerObj.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) paramsObj.set("parentNodeId", toBase64(Str("1111")));
    (void) answerObj.set("params", paramsObj);

    Poco::JSON::Object answerObjWithNumAndType = answerObj;
    (void) answerObjWithNumAndType.set("num", toInt(RequestNum::NODE_CREATEMISSINGFOLDERS));
    (void) answerObjWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    // Job expected answer
    const auto answerStr = stringifyAnswerObj(answerObjWithNumAndType);

    auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
        auto nodeCreateMissingFoldersJob = std::dynamic_pointer_cast<NodeCreateMissingFoldersJob>(job);
        CPPUNIT_ASSERT(nodeCreateMissingFoldersJob);
        CPPUNIT_ASSERT_EQUAL(1, nodeCreateMissingFoldersJob->_driveDbId);
        CPPUNIT_ASSERT(NodeId{"6666"} == nodeCreateMissingFoldersJob->_folderList.at(0).nodeId);
        CPPUNIT_ASSERT(CommString{Str("someName")} == nodeCreateMissingFoldersJob->_folderList.at(0).name);
        CPPUNIT_ASSERT(NodeId{"7777"} == nodeCreateMissingFoldersJob->_folderList.at(1).nodeId);
        CPPUNIT_ASSERT(CommString{Str("someOtherName")} == nodeCreateMissingFoldersJob->_folderList.at(1).name);

        nodeCreateMissingFoldersJob->_parentNodeId = NodeId("1111");
    };

#if defined(KD_WINDOWS) || defined(KD_LINUX)
    testGenericJob(queryStr, answerStr, {}, processFct);
#else
    const auto cbkAnswerStr = stringifyCbkAnswerObj(answerObj);
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
}
} // namespace KDC
