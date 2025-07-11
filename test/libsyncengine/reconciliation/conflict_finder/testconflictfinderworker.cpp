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

#include "testconflictfinderworker.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/testhelpers.h"

using namespace CppUnit;

namespace KDC {

void TestConflictFinderWorker::setUp() {
    TestBase::start();
    // Create SyncPal
    bool alreadyExists = false;
    const auto parmsDbPath = MockDb::makeDbName(alreadyExists);
    (void) ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    (void) std::filesystem::remove(syncDbPath);
    _syncPal = std::make_shared<SyncPal>(std::make_shared<VfsOff>(VfsSetupParams(Log::instance()->getLogger())), syncDbPath,
                                         KDRIVE_VERSION_STRING, true);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createSharedObjects();
    _syncPal->createWorkers();
    _syncPal->_conflictFinderWorker = std::make_shared<ConflictFinderWorker>(_syncPal, "Conflict Finder", "COFD");

    // Generate initial situation
    // .
    // ├── A
    // │   ├── AA
    // │   ├── AB
    // │   └── AE
    // └── B
    //     └── BE
    _situationGenerator.setSyncpal(_syncPal);
    _situationGenerator.generateInitialSituation(R"({"a":{"aa":1,"ab":1,"ae":{}},"b":{"be":{}}})");
}

void TestConflictFinderWorker::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    _syncPal->syncDb()->close();
    TestBase::stop();
}

void TestConflictFinderWorker::testCreateCreate() {
    // Simulate CREATE of A/AC on both replica
    const auto lNodeAC = _situationGenerator.createNode(ReplicaSide::Local, NodeType::File, "ac", "a");
    lNodeAC->setSize(testhelpers::defaultFileSize + 1);
    const auto rNodeAC = _situationGenerator.createNode(ReplicaSide::Remote, NodeType::File, "ac", "a");
    _syncPal->copySnapshots();
    const auto conf = _syncPal->_conflictFinderWorker->checkCreateCreateConflict(lNodeAC);
    CPPUNIT_ASSERT(conf);
    CPPUNIT_ASSERT_EQUAL(lNodeAC, conf->node());
    CPPUNIT_ASSERT_EQUAL(rNodeAC, conf->otherNode());
    CPPUNIT_ASSERT_EQUAL(ConflictType::CreateCreate, conf->type());
}

void TestConflictFinderWorker::testCreateCreateDifferentEncoding() {
    // Simulate CREATE of A/AC on both replicas
    const auto lNodeAC = _situationGenerator.createNode(ReplicaSide::Local, NodeType::File, "ac", "a");
    lNodeAC->setName(testhelpers::makeNfcSyncName());
    lNodeAC->setSize(testhelpers::defaultFileSize + 1);
    const auto rNodeAC = _situationGenerator.createNode(ReplicaSide::Remote, NodeType::File, "ac", "a");
    rNodeAC->setName(testhelpers::makeNfdSyncName());

    _syncPal->copySnapshots();
    const auto conf = _syncPal->_conflictFinderWorker->checkCreateCreateConflict(lNodeAC);
    CPPUNIT_ASSERT(conf);
    CPPUNIT_ASSERT_EQUAL(lNodeAC, conf->node());
    CPPUNIT_ASSERT_EQUAL(rNodeAC, conf->otherNode());
    CPPUNIT_ASSERT_EQUAL(ConflictType::CreateCreate, conf->type());
}

void TestConflictFinderWorker::testEditEdit() {
    // Simulate EDIT of A/AA on both replicas
    const auto lNodeAA = _situationGenerator.editNode(ReplicaSide::Local, "aa");
    const auto rNodeAA = _situationGenerator.editNode(ReplicaSide::Remote, "aa");

    _syncPal->copySnapshots();
    const auto conf = _syncPal->_conflictFinderWorker->checkEditEditConflict(lNodeAA);
    CPPUNIT_ASSERT(conf);
    CPPUNIT_ASSERT_EQUAL(lNodeAA, conf->node());
    CPPUNIT_ASSERT_EQUAL(rNodeAA, conf->otherNode());
    CPPUNIT_ASSERT_EQUAL(ConflictType::EditEdit, conf->type());
}

void TestConflictFinderWorker::testMoveCreate() {
    // Simulate CREATE of A/AC on local replica
    const auto lNodeAC = _situationGenerator.createNode(ReplicaSide::Local, NodeType::File, "ac", "a");
    // Simulate MOVE of A/AA to A/AC on remote replica
    const auto rNodeAA = _situationGenerator.renameNode(ReplicaSide::Remote, "aa", Str("AC"));

    _syncPal->copySnapshots();
    const auto conf = _syncPal->_conflictFinderWorker->checkMoveCreateConflict(rNodeAA);
    CPPUNIT_ASSERT(conf);
    CPPUNIT_ASSERT_EQUAL(rNodeAA, conf->node());
    CPPUNIT_ASSERT_EQUAL(lNodeAC, conf->otherNode());
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveCreate, conf->type());
}

void TestConflictFinderWorker::testEditDelete() {
    // Simulate EDIT of A/AA on local replica
    const auto lNodeAA = _situationGenerator.editNode(ReplicaSide::Local, "aa");
    // Simulate DELETE of A/AA on remote replica
    const auto rNodeAA = _situationGenerator.deleteNode(ReplicaSide::Remote, "aa");

    _syncPal->copySnapshots();
    const auto conf = _syncPal->_conflictFinderWorker->checkEditDeleteConflict(rNodeAA);
    CPPUNIT_ASSERT(conf);
    CPPUNIT_ASSERT_EQUAL(rNodeAA, conf->node());
    CPPUNIT_ASSERT_EQUAL(lNodeAA, conf->otherNode());
    CPPUNIT_ASSERT_EQUAL(ConflictType::EditDelete, conf->type());
}

void TestConflictFinderWorker::testMoveDeleteFile() {
    // Simulate MOVE of A/AA to B/AA on local replica
    const auto lNodeAA = _situationGenerator.moveNode(ReplicaSide::Local, "aa", "b");
    // Simulate DELETE of A/AA on remote replica
    const auto rNodeAA = _situationGenerator.deleteNode(ReplicaSide::Remote, "aa");

    _syncPal->copySnapshots();
    const auto conf = _syncPal->_conflictFinderWorker->checkMoveDeleteConflict(rNodeAA);
    CPPUNIT_ASSERT(conf);
    CPPUNIT_ASSERT_EQUAL(rNodeAA, conf->node());
    CPPUNIT_ASSERT_EQUAL(lNodeAA, conf->otherNode());
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveDelete, conf->type());
}

void TestConflictFinderWorker::testMoveDeleteDir() {
    // Simulate MOVE of A to B/A on local replica
    const auto lNodeA = _situationGenerator.moveNode(ReplicaSide::Local, "a", "b");
    // Simulate DELETE of A on remote replica
    const auto rNodeA = _situationGenerator.deleteNode(ReplicaSide::Remote, "a");

    _syncPal->copySnapshots();
    const auto conf = _syncPal->_conflictFinderWorker->checkMoveDeleteConflict(rNodeA);
    CPPUNIT_ASSERT(conf);
    CPPUNIT_ASSERT_EQUAL(rNodeA, conf->node());
    CPPUNIT_ASSERT_EQUAL(lNodeA, conf->otherNode());
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveDelete, conf->type());
}

void TestConflictFinderWorker::testMoveParentDelete() {
    // Simulate MOVE of A/AA to B/AA on local replica
    const auto lNodeAA = _situationGenerator.moveNode(ReplicaSide::Local, "aa", "b");
    // Simulate DELETE of B on remote replica
    const auto rNodeB = _situationGenerator.deleteNode(ReplicaSide::Remote, "b");

    _syncPal->copySnapshots();
    const auto conf = _syncPal->_conflictFinderWorker->checkMoveParentDeleteConflicts(rNodeB);
    CPPUNIT_ASSERT(conf);
    CPPUNIT_ASSERT_EQUAL(rNodeB, conf->back().node());
    CPPUNIT_ASSERT_EQUAL(lNodeAA, conf->back().otherNode());
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveParentDelete, conf->back().type());
}

void TestConflictFinderWorker::testCreateParentDelete() {
    // Simulate CREATE of A/AC on local replica
    const auto lNodeAC = _situationGenerator.createNode(ReplicaSide::Local, NodeType::File, "ac", "a");
    // Simulate DELETE of A on remote replica
    const auto rNodeA = _situationGenerator.deleteNode(ReplicaSide::Remote, "a");

    _syncPal->copySnapshots();
    const auto conf = _syncPal->_conflictFinderWorker->checkCreateParentDeleteConflicts(rNodeA);
    CPPUNIT_ASSERT(conf);
    CPPUNIT_ASSERT_EQUAL(rNodeA, conf->back().node());
    CPPUNIT_ASSERT_EQUAL(lNodeAC, conf->back().otherNode());
    CPPUNIT_ASSERT_EQUAL(ConflictType::CreateParentDelete, conf->back().type());
}

void TestConflictFinderWorker::testMoveMoveSrc() {
    // Simulate MOVE of A/AA to AA on local replica
    const auto lNodeAA = _situationGenerator.moveNode(ReplicaSide::Local, "aa", "");
    // Simulate MOVE of A/AA to B/AA on remote replica
    const auto rNodeAA = _situationGenerator.moveNode(ReplicaSide::Remote, "aa", "b");

    _syncPal->copySnapshots();
    const auto conf = _syncPal->_conflictFinderWorker->checkMoveMoveSourceConflict(lNodeAA);
    CPPUNIT_ASSERT(conf);
    CPPUNIT_ASSERT_EQUAL(lNodeAA, conf->node());
    CPPUNIT_ASSERT_EQUAL(rNodeAA, conf->otherNode());
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveMoveSource, conf->type());
}

void TestConflictFinderWorker::testMoveMoveDest() {
    // Simulate MOVE of A/AA to B/AA on local replica
    const auto lNodeAA = _situationGenerator.moveNode(ReplicaSide::Local, "aa", "b");
    // Simulate MOVE of A/AB to B/AB on remote replica
    const auto rNodeAB = _situationGenerator.moveNode(ReplicaSide::Remote, "ab", "b", Str("AA"));

    _syncPal->copySnapshots();
    const auto conf = _syncPal->_conflictFinderWorker->checkMoveMoveDestConflict(lNodeAA);
    CPPUNIT_ASSERT(conf);
    CPPUNIT_ASSERT_EQUAL(lNodeAA, conf->node());
    CPPUNIT_ASSERT_EQUAL(rNodeAB, conf->otherNode());
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveMoveDest, conf->type());
}

void TestConflictFinderWorker::testMoveMoveCycle() {
    // Simulate MOVE of A to B/A on local replica
    const auto lNodeA = _situationGenerator.moveNode(ReplicaSide::Local, "a", "b");
    // Simulate MOVE of B to A/B on remote replica
    const auto rNodeB = _situationGenerator.moveNode(ReplicaSide::Remote, "b", "a");

    const auto confList = _syncPal->_conflictFinderWorker->determineMoveMoveCycleConflicts({lNodeA}, {rNodeB});
    CPPUNIT_ASSERT(confList);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), confList->size());
    CPPUNIT_ASSERT_EQUAL(lNodeA, confList->back().node());
    CPPUNIT_ASSERT_EQUAL(rNodeB, confList->back().otherNode());
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveMoveCycle, confList->back().type());
}

// Edit-Edit + Move-Create : cf p93 figure 5.5 (b)
void TestConflictFinderWorker::testCase55b() {
    // Simulate EDIT of A/AA on local replica
    const auto lNodeAA = _situationGenerator.editNode(ReplicaSide::Local, "aa");
    // Simulate MOVE of A/AA to A/AC on local replica
    (void) _situationGenerator.renameNode(ReplicaSide::Local, "aa", Str("AC"));

    // Simulate EDIT of A/AA on remote replica
    const auto rNodeAA = _situationGenerator.editNode(ReplicaSide::Remote, "aa");
    // Simulate CREATE of A/AC on remote replica
    const auto rNodeAC = _situationGenerator.createNode(ReplicaSide::Remote, NodeType::File, "ac", "a");

    _syncPal->copySnapshots();
    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), _syncPal->_conflictQueue->size());

    auto conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveCreate, conf.type());
    CPPUNIT_ASSERT_EQUAL(lNodeAA, conf.node());
    CPPUNIT_ASSERT_EQUAL(rNodeAC, conf.otherNode());
    _syncPal->_conflictQueue->pop();

    conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::EditEdit, conf.type());
    CPPUNIT_ASSERT_EQUAL(lNodeAA, conf.node());
    CPPUNIT_ASSERT_EQUAL(rNodeAA, conf.otherNode());
}

// Move-Move (Source) + Move-Create + Move-Create : cf p93 figure 5.5 (c)
void TestConflictFinderWorker::testCase55c() {
    // Simulate MOVE of A/AA to A/AC on local replica
    const auto lNodeAA = _situationGenerator.renameNode(ReplicaSide::Local, "aa", Str("AC"));
    // Simulate CREATE of A/AD on local replica
    const auto lNodeAD = _situationGenerator.createNode(ReplicaSide::Local, NodeType::File, "ad", "a");

    // Simulate CREATE of A/AC on remote replica
    const auto rNodeAC = _situationGenerator.createNode(ReplicaSide::Remote, NodeType::File, "ac", "a");
    // Simulate MOVE of A/AA to A/AD on remote replica
    const auto rNodeAA = _situationGenerator.renameNode(ReplicaSide::Remote, "aa", Str("AD"));

    _syncPal->copySnapshots();

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), _syncPal->_conflictQueue->size());

    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveMoveSource, _syncPal->_conflictQueue->top().type());
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveCreate, _syncPal->_conflictQueue->top().type());
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveCreate, _syncPal->_conflictQueue->top().type());
}

// Move-ParentDelete > Move-Move (Source) : cf p96 figure 5.7 (b)
void TestConflictFinderWorker::testCase57() {
    // Simulate MOVE of A/AA to B/AA on local replica
    const auto lNodeAA = _situationGenerator.moveNode(ReplicaSide::Local, "aa", "b");

    // Simulate MOVE of A/AA to AA on remote replica
    const auto rNodeAA = _situationGenerator.moveNode(ReplicaSide::Remote, "aa", "");
    // Simulate DELETE of B on remote replica
    const auto rNodeB = _situationGenerator.deleteNode(ReplicaSide::Remote, "b");
    _syncPal->copySnapshots();

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), _syncPal->_conflictQueue->size());

    auto conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveParentDelete, conf.type());
    CPPUNIT_ASSERT_EQUAL(rNodeB, conf.node());
    CPPUNIT_ASSERT_EQUAL(_syncPal->updateTree(ReplicaSide::Remote)->rootNode(), conf.node()->parentNode());
    CPPUNIT_ASSERT_EQUAL(lNodeAA, conf.otherNode());
    const auto lNodeB = _situationGenerator.getNode(ReplicaSide::Local, "b");
    CPPUNIT_ASSERT_EQUAL(lNodeB, conf.otherNode()->parentNode());
    _syncPal->_conflictQueue->pop();

    conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveMoveSource, conf.type());
    CPPUNIT_ASSERT_EQUAL(rNodeAA, conf.node());
    CPPUNIT_ASSERT_EQUAL(_syncPal->updateTree(ReplicaSide::Remote)->rootNode(), conf.node()->parentNode());
    CPPUNIT_ASSERT_EQUAL(lNodeAA, conf.otherNode());
    CPPUNIT_ASSERT_EQUAL(lNodeB, conf.otherNode()->parentNode());
}

// Move-Delete > Move-Create : cf p98 figure 5.8
void TestConflictFinderWorker::testCase58() {
    // Simulate MOVE of A/AA to A/AC on local replica
    const auto lNodeAA = _situationGenerator.renameNode(ReplicaSide::Local, "aa", Str("AC"));

    // Simulate CREATE of A/AC on remote replica
    const auto rNodeAC = _situationGenerator.createNode(ReplicaSide::Remote, NodeType::File, "ac", "a");
    // Simulate DELETE of A/AA on remote replica
    const auto rNodeAA = _situationGenerator.deleteNode(ReplicaSide::Remote, "aa");
    _syncPal->copySnapshots();

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), _syncPal->_conflictQueue->size());

    auto conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveDelete, conf.type());
    CPPUNIT_ASSERT_EQUAL(rNodeAA, conf.node());
    CPPUNIT_ASSERT_EQUAL(lNodeAA, conf.otherNode());
    _syncPal->_conflictQueue->pop();

    conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveCreate, conf.type());
    CPPUNIT_ASSERT_EQUAL(lNodeAA, conf.node());
    CPPUNIT_ASSERT_EQUAL(rNodeAC, conf.otherNode());
}

// Move-Delete > Move-Move (Dest) : cf p98 figure 5.9 (b)
void TestConflictFinderWorker::testCase59() {
    // Simulate MOVE of A/AB to A/X on local replica
    const auto lNodeAB = _situationGenerator.renameNode(ReplicaSide::Local, "ab", Str("X"));

    // Simulate DELETE of A/AB on remote replica
    const auto rNodeAB = _situationGenerator.deleteNode(ReplicaSide::Remote, "ab");
    // Simulate MOVE of A/AA to A/X on remote replica
    const auto rNodeAA = _situationGenerator.renameNode(ReplicaSide::Remote, "aa", Str("X"));
    _syncPal->copySnapshots();

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), _syncPal->_conflictQueue->size());

    auto conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT(conf.type() == ConflictType::MoveDelete);
    CPPUNIT_ASSERT_EQUAL(rNodeAB, conf.node());
    CPPUNIT_ASSERT_EQUAL(lNodeAB, conf.otherNode());
    _syncPal->_conflictQueue->pop();

    conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT(conf.type() == ConflictType::MoveMoveDest);
    // Because of how ConflictFinderWorker is implemented, if both operations have the same change event, local node will be
    // stored in `node` and remote node will be stored in `otherNode`
    CPPUNIT_ASSERT_EQUAL(lNodeAB, conf.node());
    CPPUNIT_ASSERT_EQUAL(rNodeAA, conf.otherNode());
}

// Move-Delete > Create-ParentDelete : cf p99 figure 5.10
void TestConflictFinderWorker::testCase510() {
    // Simulate MOVE of A/AA to AA on local replica
    const auto lNodeAA = _situationGenerator.moveNode(ReplicaSide::Local, "aa", "");
    // Simulate CREATE of A/AA on local replica
    const auto lNodeAA2 = _situationGenerator.createNode(ReplicaSide::Local, NodeType::File, "aa2", "a");
    lNodeAA2->setName(Str("AA"));

    // Simulate DELETE of A on remote replica
    const auto rNodeA = _situationGenerator.deleteNode(ReplicaSide::Remote, "a");
    _syncPal->copySnapshots();

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), _syncPal->_conflictQueue->size());

    auto conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveDelete, conf.type());
    const auto rNodeAA = _situationGenerator.getNode(ReplicaSide::Remote, "aa");
    CPPUNIT_ASSERT_EQUAL(rNodeAA, conf.node());
    CPPUNIT_ASSERT_EQUAL(lNodeAA, conf.otherNode());
    _syncPal->_conflictQueue->pop();

    conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::CreateParentDelete, conf.type());
    CPPUNIT_ASSERT_EQUAL(rNodeA, conf.node());
    CPPUNIT_ASSERT_EQUAL(lNodeAA2, conf.otherNode());
}

// Move-Delete > Create-ParentDelete : cf p100 figure 5.11
void TestConflictFinderWorker::testCase511() {
    // Simulate DELETE of A on local replica
    const auto lNodeA = _situationGenerator.deleteNode(ReplicaSide::Local, "a");

    // Simulate MOVE of A/AE to AE on remote replica
    const auto rNodeAE = _situationGenerator.moveNode(ReplicaSide::Remote, "ae", "");
    // Simulate CREATE of AE/AEA on remote replica
    const auto rNodeAEA = _situationGenerator.createNode(ReplicaSide::Remote, NodeType::File, "aea", "ae");
    _syncPal->copySnapshots();

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), _syncPal->_conflictQueue->size());

    auto conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveDelete, conf.type());
    const auto lNodeAE = _situationGenerator.getNode(ReplicaSide::Local, "ae");
    CPPUNIT_ASSERT_EQUAL(conf.node(), lNodeAE);
    CPPUNIT_ASSERT_EQUAL(rNodeAE, conf.otherNode());
    _syncPal->_conflictQueue->pop();

    conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::CreateParentDelete, conf.type());
    CPPUNIT_ASSERT_EQUAL(lNodeAE, conf.node());
    CPPUNIT_ASSERT_EQUAL(rNodeAEA, conf.otherNode());
}

// Move-Move (Source) turns to Move-Move (Cycle) : cf p103 figure 5.13
void TestConflictFinderWorker::testCase513() {
    // Step1 : detection of the Move-Move (Source) conflict : cf p103 figure 5.13 (b)
    // Simulate MOVE of A/AE to AE on local replica
    const auto lNodeAE = _situationGenerator.moveNode(ReplicaSide::Local, "ae", "", Str("AE1"));
    // Simulate MOVE of B to AE/B on local replica
    const auto lNodeB = _situationGenerator.moveNode(ReplicaSide::Local, "b", "ae");

    // Simulate MOVE of A/AE to AE on remote replica
    const auto rNodeAE = _situationGenerator.moveNode(ReplicaSide::Remote, "ae", "", Str("AE2"));
    // Simulate MOVE of A to B/A on remote replica
    const auto rNodeA = _situationGenerator.moveNode(ReplicaSide::Remote, "a", "b");
    _syncPal->copySnapshots();

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_conflictQueue->size());

    auto conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveMoveSource, conf.type());
    CPPUNIT_ASSERT_EQUAL(lNodeAE, conf.node());
    CPPUNIT_ASSERT_EQUAL(rNodeAE, conf.otherNode());

    // Step2 : Move-Move (Source) conflict is resolved by undoing local move operation of AE, generating a Move-Move (Cycle)
    // conflict situation : cf p103 figure 5.13 (c)
    _syncPal->_conflictQueue->clear();

    // Cancel the move of local node AE
    (void) _situationGenerator.moveNode(ReplicaSide::Local, "ae", "a", Str("AE"));
    lNodeAE->setChangeEvents(OperationType::None);

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_conflictQueue->size());

    conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveMoveCycle, conf.type());
    CPPUNIT_ASSERT_EQUAL(lNodeB, conf.node());
    CPPUNIT_ASSERT_EQUAL(rNodeA, conf.otherNode());
}
// Move-Move (Cycle) introduces new Move-Move : cf p106 figure 5.16
void TestConflictFinderWorker::testCase516() {
    // Step1 : Detection of the Move-Move (Cycle) conflict : cf p106 figure 5.16 (b)
    // Simulate MOVE of A/AE to AE on local replica
    const auto lNodeAE = _situationGenerator.moveNode(ReplicaSide::Local, "ae", "");
    // Simulate MOVE of B/BE to AE/BE on local replica
    const auto lNodeBE = _situationGenerator.moveNode(ReplicaSide::Local, "be", "ae");
    // Simulate MOVE of A to AE/BE/A on local replica
    const auto lNodeA = _situationGenerator.moveNode(ReplicaSide::Local, "a", "be");

    // Simulate MOVE of B/BE to BE on remote replica
    const auto rNodeBE = _situationGenerator.moveNode(ReplicaSide::Remote, "be", "");
    // Simulate MOVE of B to A/B on remote replica
    const auto rNodeB = _situationGenerator.moveNode(ReplicaSide::Remote, "b", "a");
    // Simulate MOVE of A/AE to BE/AE on remote replica
    const auto rNodeAE = _situationGenerator.moveNode(ReplicaSide::Remote, "ae", "be");
    _syncPal->copySnapshots();

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), _syncPal->_conflictQueue->size());

    auto conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveMoveSource, conf.type());
    CPPUNIT_ASSERT_EQUAL(lNodeAE, conf.node());
    CPPUNIT_ASSERT_EQUAL(rNodeAE, conf.otherNode());
    _syncPal->_conflictQueue->pop();

    conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveMoveSource, conf.type());
    CPPUNIT_ASSERT_EQUAL(rNodeBE, conf.node());
    CPPUNIT_ASSERT_EQUAL(lNodeBE, conf.otherNode());
    _syncPal->_conflictQueue->pop();

    conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveMoveCycle, conf.type());
    CPPUNIT_ASSERT_EQUAL(lNodeBE, conf.node());
    CPPUNIT_ASSERT_EQUAL(rNodeAE, conf.otherNode());

    // Step2 : Detection of Move-Move (Cycle) conflict : cf p106 figure 5.16 (c)
    _syncPal->_conflictQueue->clear();

    // Cancel the move of local node BE
    (void) _situationGenerator.moveNode(ReplicaSide::Local, "be", "b");
    lNodeBE->setChangeEvents(OperationType::None);

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), _syncPal->_conflictQueue->size());

    conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveMoveSource, conf.type());
    CPPUNIT_ASSERT_EQUAL(lNodeAE, conf.node());
    CPPUNIT_ASSERT_EQUAL(rNodeAE, conf.otherNode());
    _syncPal->_conflictQueue->pop();

    conf = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveMoveCycle, conf.type());
    CPPUNIT_ASSERT_EQUAL(lNodeA, conf.node());
    CPPUNIT_ASSERT_EQUAL(rNodeB, conf.otherNode());
}

void TestConflictFinderWorker::testConflictCmp() {
    const ConflictCmp cmp(_syncPal->updateTree(ReplicaSide::Local), _syncPal->updateTree(ReplicaSide::Remote));
    constexpr std::array conflictTypes = {ConflictType::MoveParentDelete, ConflictType::CreateParentDelete,
                                          ConflictType::MoveDelete,       ConflictType::EditDelete,
                                          ConflictType::MoveMoveSource,   ConflictType::MoveMoveDest,
                                          ConflictType::MoveMoveCycle,    ConflictType::CreateCreate,
                                          ConflictType::EditEdit,         ConflictType::MoveCreate};

    ConflictQueue queue(_syncPal->updateTree(ReplicaSide::Local), _syncPal->updateTree(ReplicaSide::Remote));

    const OperationType allOp = OperationType::Create | OperationType::Edit | OperationType::Delete | OperationType::Move;
    const auto lNodeAA = _situationGenerator.getNode(ReplicaSide::Local, "aa");
    lNodeAA->setMoveOriginInfos({lNodeAA->getPath(), *lNodeAA->parentNode()->id()});
    lNodeAA->setChangeEvents(allOp);
    const auto rNodeAA = _situationGenerator.getNode(ReplicaSide::Remote, "aa");
    rNodeAA->setMoveOriginInfos({rNodeAA->getPath(), *rNodeAA->parentNode()->id()});
    rNodeAA->setChangeEvents(allOp);

    constexpr auto nbConflictType = conflictTypes.size();
    std::random_device rd; // Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); // Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<size_t> dis(1, nbConflictType);
    for (size_t i = 0; i < 100; i++) {
        const auto index = dis(gen) % nbConflictType;
        const Conflict c1(lNodeAA, rNodeAA, conflictTypes[index]);
    }

    // Ensure that all the operations are grouped and sorted by type
    auto lastType = ConflictType::None;
    while (!queue.empty()) {
        const ConflictType currentType = queue.top().type();
        CPPUNIT_ASSERT(currentType >= lastType);
        lastType = currentType;
        queue.pop();
    }
}
} // namespace KDC
