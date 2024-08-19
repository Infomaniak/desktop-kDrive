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

#include "testplatforminconsistencycheckerworker.h"

#include <memory>
#include "libcommonserver/utility/utility.h"
#include "libcommon/utility/types.h"
#include "requests/parameterscache.h"
#include "syncpal/tmpblacklistmanager.h"

#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#include "test_utility/localtemporarydirectory.h"

using namespace CppUnit;

namespace KDC {

#if defined(_WIN32)
#define FORBIDDEN_FILENAME_CHARS "\\/:*?\"<>|"
#elif defined(__APPLE__)
#define FORBIDDEN_FILENAME_CHARS "/:"
#else
#define FORBIDDEN_FILENAME_CHARS "/\0"
#endif

#define MAX_PATH_LENGTH_WIN_SHORT 255
#define MAX_NAME_LENGTH_WIN_SHORT 255

void TestPlatformInconsistencyCheckerWorker::setUp() {
    // Create parmsDb
    bool alreadyExists = false;
    const auto parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);

    // Insert user, account, drive & sync
    const User user(1, 1, "dummy");
    ParmsDb::instance()->insertUser(user);

    const Account account(1, 1, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    const Drive drive(1, 1, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);

    const Sync sync(1, drive.dbId(), _tempDir.path(), "");
    ParmsDb::instance()->insertSync(sync);

    // Create SyncPal
    _syncPal = std::make_shared<SyncPal>(sync.dbId(), "3.4.0");
    _syncPal->_syncDb->setAutoDelete(true);
    _syncPal->_tmpBlacklistManager = std::make_shared<TmpBlacklistManager>(_syncPal);

    _syncPal->_platformInconsistencyCheckerWorker =
        std::make_shared<PlatformInconsistencyCheckerWorker>(_syncPal, "Platform Inconsistency Checker", "PICH");
}

void TestPlatformInconsistencyCheckerWorker::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
}

void TestPlatformInconsistencyCheckerWorker::testFixNameSize() {
    SyncName shortName = Str("1234567890");
    CPPUNIT_ASSERT(!PlatformInconsistencyCheckerUtility::instance()->checkNameSize(shortName));

    SyncName longName =
        Str("12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456"
            "78901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012"
            "3456789012345678901234567890");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkNameSize(longName));
}

void TestPlatformInconsistencyCheckerWorker::testCheckNameForbiddenChars() {
    SyncName allowedName = Str("test-test");
    CPPUNIT_ASSERT(!PlatformInconsistencyCheckerUtility::instance()->checkNameForbiddenChars(allowedName));

    SyncName forbiddenName = Str("test/test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkNameForbiddenChars(forbiddenName));

#if defined(WIN32)
    forbiddenName = Str("test\\test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkNameForbiddenChars(forbiddenName));
    forbiddenName = Str("test:test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkNameForbiddenChars(forbiddenName));
    forbiddenName = Str("test*test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkNameForbiddenChars(forbiddenName));
    forbiddenName = Str("test?test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkNameForbiddenChars(forbiddenName));
    forbiddenName = Str("test\"test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkNameForbiddenChars(forbiddenName));
    forbiddenName = Str("test<test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkNameForbiddenChars(forbiddenName));
    forbiddenName = Str("test>test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkNameForbiddenChars(forbiddenName));
    forbiddenName = Str("test|test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkNameForbiddenChars(forbiddenName));
    forbiddenName = Str("test\ntest");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkNameForbiddenChars(forbiddenName));
    forbiddenName = Str("test ");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkNameForbiddenChars(forbiddenName));
#elif defined(__unix__) && !defined(__APPLE__)
    forbiddenName = std::string("test");
    forbiddenName.append(1, '\0');
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkNameForbiddenChars(forbiddenName));
#endif
}

void TestPlatformInconsistencyCheckerWorker::testCheckReservedNames() {
    SyncName allowedName = Str("....text");
    CPPUNIT_ASSERT(!PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(allowedName));

    std::array<SyncName, 2> reservedNames{{Str(".."), Str(".")}};
    for (const auto &name : reservedNames) {
        CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(name));
    }

#if defined(WIN32)
    std::array<SyncName, 7> reservedWinNames{
        {Str("...."), Str("CON"), Str("LPT5"), Str("COM8"), Str("NUL"), Str("AUX"), Str("test.")}};

    for (const auto &name : reservedWinNames) {
        CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(name));
    }
#endif
}

void TestPlatformInconsistencyCheckerWorker::testNameClash() {
    const auto parentNode = std::make_shared<Node>(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(),
                                                   Str("parentNode"), NodeType::Directory, OperationType::Create, "parentID", 0,
                                                   0, 12345, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());

    const auto nodeLower = std::make_shared<Node>(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("a"),
                                                  NodeType::File, OperationType::Create, "a", 0, 0, 12345, parentNode);
    const auto nodeUpper = std::make_shared<Node>(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("A"),
                                                  NodeType::File, OperationType::Create, "A", 0, 0, 12345, parentNode);

    CPPUNIT_ASSERT(parentNode->insertChildren(nodeLower));
    CPPUNIT_ASSERT(parentNode->insertChildren(nodeUpper));

    _syncPal->_platformInconsistencyCheckerWorker->checkNameClashAgainstSiblings(parentNode);

#if defined(WIN32) || defined(__APPLE__)
    CPPUNIT_ASSERT(!_syncPal->_platformInconsistencyCheckerWorker->_idsToBeRemoved.empty());
#else
    CPPUNIT_ASSERT(_syncPal->_platformInconsistencyCheckerWorker->_idsToBeRemoved.empty());
#endif
}

void TestPlatformInconsistencyCheckerWorker::testNameClashAfterRename() {
    // Create local files
    for (const std::vector<std::string> nodes = {{"a1"}, {"A"}}; const auto &n : nodes) {
        std::ofstream ofs(_tempDir.path() / n);
        ofs << "Some content.\n";
        ofs.close();
    }

    // Set up DB
    const DbNode dbNodeLower(2, _syncPal->_syncDb->rootNode().nodeId(), Str("a1"), Str("a1"), "la", "ra", defaultTime,
                             defaultTime, defaultTime, NodeType::Directory, defaultSize, std::nullopt);
    const DbNode dbNodeUpper(3, _syncPal->_syncDb->rootNode().nodeId(), Str("A"), Str("A"), "lA", "rA", defaultTime, defaultTime,
                             defaultTime, NodeType::Directory, defaultSize, std::nullopt);
    DbNodeId dbNodeIdLower;
    DbNodeId dbNodeIdUpper;
    bool constraintError = false;
    _syncPal->_syncDb->insertNode(dbNodeLower, dbNodeIdLower, constraintError);
    _syncPal->_syncDb->insertNode(dbNodeUpper, dbNodeIdUpper, constraintError);

    // Set up remote tree
    const auto remoteParentNode = _syncPal->updateTree(ReplicaSide::Remote)->rootNode();
    const auto remoteNodeLower =
        std::make_shared<Node>(dbNodeIdLower, ReplicaSide::Remote, Str("a"), NodeType::File, OperationType::Move, "ra", 0, 0,
                               12345, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto remoteNodeUpper =
        std::make_shared<Node>(dbNodeIdUpper, ReplicaSide::Remote, Str("A"), NodeType::File, OperationType::None, "rA", 0, 0,
                               12345, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());

    CPPUNIT_ASSERT(remoteParentNode->insertChildren(remoteNodeLower));
    CPPUNIT_ASSERT(remoteParentNode->insertChildren(remoteNodeUpper));

    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(remoteNodeLower);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(remoteNodeUpper);

    // Set up local tree
    const auto localParentNode = _syncPal->updateTree(ReplicaSide::Local)->rootNode();
    const auto localNodeLower = std::make_shared<Node>(dbNodeIdLower, ReplicaSide::Local, Str("a1"), NodeType::File,
                                                       OperationType::None, "la", 0, 0, 12345, localParentNode);
    const auto localNodeUpper = std::make_shared<Node>(dbNodeIdUpper, ReplicaSide::Local, Str("A"), NodeType::File,
                                                       OperationType::None, "lA", 0, 0, 12345, localParentNode);

    CPPUNIT_ASSERT(localParentNode->insertChildren(localNodeLower));
    CPPUNIT_ASSERT(localParentNode->insertChildren(localNodeUpper));

    _syncPal->updateTree(ReplicaSide::Local)->insertNode(localNodeLower);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(localNodeUpper);

    // Check name clash
    _syncPal->_platformInconsistencyCheckerWorker->checkNameClashAgainstSiblings(remoteParentNode);

#if defined(WIN32) || defined(__APPLE__)
    CPPUNIT_ASSERT(!_syncPal->_platformInconsistencyCheckerWorker->_idsToBeRemoved.empty());
    CPPUNIT_ASSERT(!std::filesystem::exists(_tempDir.path() / "a1"));
    std::error_code ec;
    auto dirIt = std::filesystem::recursive_directory_iterator(_syncPal->_localPath,
                                                               std::filesystem::directory_options::skip_permission_denied, ec);
    CPPUNIT_ASSERT(!ec);
    bool foundConflicted = false;
    for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
        const auto filename = dirIt->path().filename().string();
        const auto pos = filename.find("_conflict_");
        if (Utility::startsWith(filename, std::string("a1")) && pos != std::string::npos) {
            foundConflicted = true;
            break;
        }
    }
    CPPUNIT_ASSERT(foundConflicted);

#else
    CPPUNIT_ASSERT(_syncPal->_platformInconsistencyCheckerWorker->_idsToBeRemoved.empty());
#endif
}

void TestPlatformInconsistencyCheckerWorker::testExecute() {
    const auto parentNode = std::make_shared<Node>(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(),
                                                   Str("parentNode"), NodeType::Directory, OperationType::Create, "parentID", 0,
                                                   0, 12345, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());

    const auto nodeLower = std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("a"), NodeType::File,
                                                  OperationType::Create, "a", 0, 0, 12345, parentNode);
    const auto nodeUpper = std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("A"), NodeType::File,
                                                  OperationType::Create, "A", 0, 0, 12345, parentNode);

    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(parentNode));
    CPPUNIT_ASSERT(parentNode->insertChildren(nodeLower));
    CPPUNIT_ASSERT(parentNode->insertChildren(nodeUpper));

    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(parentNode);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(nodeLower);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(nodeUpper);

    _syncPal->_platformInconsistencyCheckerWorker->execute();

    const bool exactly1exist = (_syncPal->updateTree(ReplicaSide::Remote)->exists(*nodeUpper->id()) &&
                                !_syncPal->updateTree(ReplicaSide::Remote)->exists(*nodeLower->id())) ||
                               (!_syncPal->updateTree(ReplicaSide::Remote)->exists(*nodeUpper->id()) &&
                                _syncPal->updateTree(ReplicaSide::Remote)->exists(*nodeLower->id()));

    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->exists(*parentNode->id()));

#if defined(WIN32) || defined(__APPLE__)
    CPPUNIT_ASSERT(exactly1exist);
#else
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->exists(*nodeUpper->id()) &&
                   _syncPal->updateTree(ReplicaSide::Remote)->exists(*nodeLower->id()));
#endif
}

}  // namespace KDC
