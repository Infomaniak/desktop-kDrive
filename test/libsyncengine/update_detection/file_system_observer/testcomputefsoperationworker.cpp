/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "testcomputefsoperationworker.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommon/utility/utility.h"
#include "requests/exclusiontemplatecache.h"
#include "requests/syncnodecache.h"

#include <update_detection/file_system_observer/computefsoperationworker.h>

namespace KDC {

static const time_t tLoc = std::time(0);
static const time_t tDrive = std::time(0);

/**
 * init tree:
 *
 *                   root
 *          __________|__________
 *         |                    |
 *         A                    B
 *    _____|_____          _____|_____
 *   |     |    |         |          |
 *  AA    AB   AC        AA         AB

 */

void TestComputeFSOperationWorker::setUp() {
    const std::string userIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_USER_ID");
    const std::string accountIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_ACCOUNT_ID");
    const std::string driveIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_DRIVE_ID");
    const std::string localPathStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_LOCAL_PATH");
    const std::string remotePathStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_REMOTE_PATH");
    const std::string apiTokenStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_API_TOKEN");

    if (userIdStr.empty() || accountIdStr.empty() || driveIdStr.empty() || localPathStr.empty() || remotePathStr.empty() ||
        apiTokenStr.empty()) {
        throw std::runtime_error("Some environment variables are missing!");
    }

    // Insert api token into keystore
    std::string keychainKey("123");
    KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, apiTokenStr);

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParmsDb::instance()->setAutoDelete(true);

    // Insert user, account, drive & sync
    int userId = atoi(userIdStr.c_str());
    User user(1, userId, keychainKey);
    ParmsDb::instance()->insertUser(user);

    int accountId(atoi(accountIdStr.c_str()));
    Account account(1, accountId, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    int driveDbId = 1;
    int driveId = atoi(driveIdStr.c_str());
    Drive drive(driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);

    Sync sync(1, drive.dbId(), localPathStr, remotePathStr);
    ParmsDb::instance()->insertSync(sync);

    _syncPal = std::shared_ptr<SyncPal>(new SyncPal(sync.dbId(), "3.4.0"));
    _syncPal->_syncDb->setAutoDelete(true);

    /// Insert nodes in DB
    DbNode nodeDirA(0, _syncPal->_syncDb->rootNode().nodeId(), Str("A"), Str("A"), "la", "ra", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    DbNode nodeDirB(0, _syncPal->_syncDb->rootNode().nodeId(), Str("B"), Str("B"), "lb", "rb", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    DbNodeId dbNodeIdDirA;
    DbNodeId dbNodeIdDirB;
    bool constraintError = false;
    _syncPal->_syncDb->insertNode(nodeDirA, dbNodeIdDirA, constraintError);
    _syncPal->_syncDb->insertNode(nodeDirB, dbNodeIdDirB, constraintError);

    DbNode nodeFileAA(0, dbNodeIdDirA, Str("AA"), Str("AA"), "laa", "raa", tLoc, tLoc, tDrive, NodeType::NodeTypeFile, 0,
                      "cs_aa");
    DbNode nodeFileAB(0, dbNodeIdDirA, Str("AB"), Str("AB"), "lab", "rab", tLoc, tLoc, tDrive, NodeType::NodeTypeFile, 0,
                      "cs_ab");
    DbNode nodeFileAC(0, dbNodeIdDirA, Str("AC"), Str("AC"), "lac", "rac", tLoc, tLoc, tDrive, NodeType::NodeTypeFile, 0,
                      "cs_ac");
    DbNode nodeFileBA(0, dbNodeIdDirB, Str("BA"), Str("BA"), "lba", "rba", tLoc, tLoc, tDrive, NodeType::NodeTypeFile, 0,
                      "cs_ba");
    DbNode nodeFileBB(0, dbNodeIdDirB, Str("BB"), Str("BB"), "lbb", "rbb", tLoc, tLoc, tDrive, NodeType::NodeTypeFile, 0,
                      "cs_bb");
    DbNodeId dbNodeIdFileAA;
    DbNodeId dbNodeIdFileAB;
    DbNodeId dbNodeIdFileBA;
    DbNodeId dbNodeIdFileBB;
    _syncPal->_syncDb->insertNode(nodeFileAA, dbNodeIdFileAA, constraintError);
    _syncPal->_syncDb->insertNode(nodeFileAB, dbNodeIdFileAB, constraintError);
    // AC not in db since it should be excluded from sync
    _syncPal->_syncDb->insertNode(nodeFileBA, dbNodeIdFileBA, constraintError);
    _syncPal->_syncDb->insertNode(nodeFileBB, dbNodeIdFileBB, constraintError);

    // Init test snapshot
    /// Insert dir
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem(nodeDirA.nodeIdLocal().value(), _syncPal->_syncDb->rootNode().nodeIdLocal().value(), nodeDirA.nameLocal(),
                     nodeDirA.created().value(), nodeDirA.lastModifiedLocal().value(), nodeDirA.type(), 123));
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem(nodeDirB.nodeIdLocal().value(), _syncPal->_syncDb->rootNode().nodeIdLocal().value(), nodeDirB.nameLocal(),
                     nodeDirB.created().value(), nodeDirB.lastModifiedLocal().value(), nodeDirB.type(), 123));

    _syncPal->_remoteSnapshot->updateItem(
        SnapshotItem(nodeDirA.nodeIdRemote().value(), _syncPal->_syncDb->rootNode().nodeIdRemote().value(), nodeDirA.nameRemote(),
                     nodeDirA.created().value(), nodeDirA.lastModifiedRemote().value(), nodeDirA.type(),
                     550 * 1024 * 1024));  // File size: 550MB

    /// Insert files
    _syncPal->_localSnapshot->updateItem(SnapshotItem(nodeFileAA.nodeIdLocal().value(), nodeDirA.nodeIdLocal().value(),
                                                      nodeFileAA.nameLocal(), nodeFileAA.created().value(),
                                                      nodeFileAA.lastModifiedLocal().value(), nodeFileAA.type(), 123));
    _syncPal->_localSnapshot->updateItem(SnapshotItem(nodeFileAB.nodeIdLocal().value(), nodeDirA.nodeIdLocal().value(),
                                                      nodeFileAB.nameLocal(), nodeFileAB.created().value(),
                                                      nodeFileAB.lastModifiedLocal().value(), nodeFileAB.type(), 123));
    _syncPal->_localSnapshot->updateItem(SnapshotItem(nodeFileBA.nodeIdLocal().value(), nodeDirB.nodeIdLocal().value(),
                                                      nodeFileBA.nameLocal(), nodeFileBA.created().value(),
                                                      nodeFileBA.lastModifiedLocal().value(), nodeFileBA.type(), 123));
    _syncPal->_localSnapshot->updateItem(SnapshotItem(nodeFileBB.nodeIdLocal().value(), nodeDirB.nodeIdLocal().value(),
                                                      nodeFileBB.nameLocal(), nodeFileBB.created().value(),
                                                      nodeFileBB.lastModifiedLocal().value(), nodeFileBB.type(), 123));

    _syncPal->_remoteSnapshot->updateItem(SnapshotItem(nodeFileAC.nodeIdRemote().value(), nodeDirA.nodeIdRemote().value(),
                                                       nodeFileAC.nameRemote(), nodeFileAC.created().value(),
                                                       nodeFileAC.lastModifiedLocal().value(), nodeFileAC.type(), 123));

    // Insert items to excluded templates in DB
    std::vector<ExclusionTemplate> templateVec = {
        ExclusionTemplate(".DS_Store", true)  // TODO : to be removed once we have a default list of file excluded implemented
        ,
        ExclusionTemplate("*_conflict_*_*_*",
                          true)  // TODO : to be removed once we have a default list of file excluded implemented
        ,
        ExclusionTemplate("*_excluded", true)};
    ExclusionTemplateCache::instance()->update(true, templateVec);
    // Insert items to blacklist
    SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeTypeBlackList, {"lac"});

    _syncPal->_computeFSOperationsWorker =
        std::shared_ptr<ComputeFSOperationWorker>(new ComputeFSOperationWorker(_syncPal, "Local Compute FS Operations", "LCOP"));
}

void TestComputeFSOperationWorker::tearDown() {
    ParmsDb::reset();
}

void TestComputeFSOperationWorker::testComputeOps() {
    _syncPal->_computeFSOperationsWorker->execute();
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->ops().empty());

    // On local replica
    // Create operation
    _syncPal->_localSnapshot->updateItem(SnapshotItem("lad", "la", Str("AD"), tLoc, tLoc, NodeTypeFile, 123));
    // Edit operation
    _syncPal->_localSnapshot->setLastModified("laa", tLoc + 60);
    // Move operation
    _syncPal->_localSnapshot->setParentId("lab", "lb");
    // Rename operation
    _syncPal->_localSnapshot->setName("lba", Str("BA-renamed"));
    // Delete operation
    _syncPal->_localSnapshot->removeItem("lbb");

    // Create operation on an excluded file
    _syncPal->_localSnapshot->updateItem(SnapshotItem("lae", "la", Str("AE_excluded"), tLoc, tLoc, NodeTypeFile, 123));
    // Create operation but file too big (should be ignored on local replica)
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem("laf", "la", Str("AF_too_big"), tLoc, tLoc, NodeTypeFile, 550 * 1024 * 1024));  // File size: 550MB

    // Rename operation on a blacklisted directory
    _syncPal->_remoteSnapshot->setName("rac", Str("AC-blacklisted"));

    _syncPal->_computeFSOperationsWorker->execute();

    FSOpPtr tmpOp = nullptr;
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->findOp("lad", OperationTypeCreate, tmpOp));
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->findOp("laa", OperationTypeEdit, tmpOp));
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->findOp("lab", OperationTypeMove, tmpOp));
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->findOp("lba", OperationTypeMove, tmpOp));
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->findOp("lbb", OperationTypeDelete, tmpOp));

    CPPUNIT_ASSERT(!_syncPal->_localOperationSet->findOp("lae", OperationTypeCreate, tmpOp));

    CPPUNIT_ASSERT(!_syncPal->_localOperationSet->findOp("laf", OperationTypeCreate, tmpOp));

    // On remote replica
    // Create operation but file too big (should be ignored on local replica)
    CPPUNIT_ASSERT(!_syncPal->_localOperationSet->findOp("ra", OperationTypeCreate, tmpOp));

    CPPUNIT_ASSERT(!_syncPal->_localOperationSet->findOp("rac", OperationTypeMove, tmpOp));
}

}  // namespace KDC
