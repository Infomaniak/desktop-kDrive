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
#include "testexecutorworker.h"

using namespace CppUnit;

namespace KDC {

static const SyncTime defaultTime = 1654788079;
static const int64_t defaultSize = 1654788079;

void TestExecutorWorker::setUp() {
    // Create SyncPal
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);

    SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    std::filesystem::remove(syncDbPath);
    _syncPal = std::make_shared<SyncPal>(syncDbPath, "3.4.0", true);
    _testObj = std::make_shared<ExecutorWorker>(_syncPal, "name", "short name");

    _syncPal->syncDb()->setAutoDelete(true);
}

void TestExecutorWorker::tearDown() {
    _syncPal->stop(false, true, false);
    _testObj.reset();
    _syncPal.reset();
    ParmsDb::reset();
}

void TestExecutorWorker::testAffectedUpdateTree() {
    // Normal cases
    auto syncOp = std::make_shared<SyncOperation>();
    syncOp->setTargetSide(ReplicaSideLocal);
    CPPUNIT_ASSERT_EQUAL(ReplicaSideRemote, _testObj->affectedUpdateTree(syncOp)->side());

    syncOp->setTargetSide(ReplicaSideRemote);
    CPPUNIT_ASSERT_EQUAL(ReplicaSideLocal, _testObj->affectedUpdateTree(syncOp)->side());

    // ReplicaSideUnknown case
    syncOp->setTargetSide(ReplicaSideUnknown);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<UpdateTree>(nullptr), _testObj->affectedUpdateTree(syncOp));
}

void TestExecutorWorker::testTargetUpdateTree() {
    // Normal cases
    auto syncOp = std::make_shared<SyncOperation>();
    syncOp->setTargetSide(ReplicaSideLocal);
    CPPUNIT_ASSERT_EQUAL(ReplicaSideLocal, _testObj->targetUpdateTree(syncOp)->side());

    syncOp->setTargetSide(ReplicaSideRemote);
    CPPUNIT_ASSERT_EQUAL(ReplicaSideRemote, _testObj->targetUpdateTree(syncOp)->side());

    // ReplicaSideUnknown case
    syncOp->setTargetSide(ReplicaSideUnknown);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<UpdateTree>(nullptr), _testObj->targetUpdateTree(syncOp));
}
}  // namespace KDC
