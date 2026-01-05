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

#include "testplatforminconsistencycheckerworker.h"
#include "syncpal/tmpblacklistmanager.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#include "libcommon/utility/types.h"
#include "libcommonserver/utility/utility.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/localtemporarydirectory.h"
#include "test_utility/testhelpers.h"

#include <memory>

using namespace CppUnit;

namespace KDC {

#if defined(KD_WINDOWS)
#define FORBIDDEN_FILENAME_CHARS "\\/:*?\"<>|"
#elif defined(KD_MACOS)
#define FORBIDDEN_FILENAME_CHARS "/:"
#else
#define FORBIDDEN_FILENAME_CHARS "/\0"
#endif

#define MAX_PATH_LENGTH_WIN_SHORT 255
#define MAX_NAME_LENGTH_WIN_SHORT 255

void TestPlatformInconsistencyCheckerWorker::setUp() {
    TestBase::start();
    // Create parmsDb
    (void) ParmsDb::instance(_localTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync
    const User user(1, 1, "dummy");
    (void) ParmsDb::instance()->insertUser(user);

    const Account account(1, 1, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    const Drive drive(1, 1, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    const Sync sync(1, drive.dbId(), _tempDir.path(), "", "", "");
    (void) ParmsDb::instance()->insertSync(sync);

    // Create SyncPal
    _syncPal = std::make_shared<SyncPal>(std::make_shared<VfsOff>(VfsSetupParams(Log::instance()->getLogger())), sync.dbId(),
                                         KDRIVE_VERSION_STRING);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createSharedObjects();
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
    TestBase::stop();
}

void TestPlatformInconsistencyCheckerWorker::testIsNameTooLong() {
    SyncName shortName = Str("1234567890");
    CPPUNIT_ASSERT(!PlatformInconsistencyCheckerUtility::instance()->isNameTooLong(shortName));

    SyncName longName = Str(
            "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456"
            "78901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012"
            "3456789012345678901234567890");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->isNameTooLong(longName));
}

void TestPlatformInconsistencyCheckerWorker::testCheckNameForbiddenChars() {
    SyncName allowedName = Str("test-test");
    CPPUNIT_ASSERT(!PlatformInconsistencyCheckerUtility::instance()->nameHasForbiddenChars(allowedName));

    SyncName forbiddenName = Str("test/test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->nameHasForbiddenChars(forbiddenName));

#if defined(KD_WINDOWS)
    forbiddenName = Str("test\\test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->nameHasForbiddenChars(forbiddenName));
    forbiddenName = Str("test:test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->nameHasForbiddenChars(forbiddenName));
    forbiddenName = Str("test*test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->nameHasForbiddenChars(forbiddenName));
    forbiddenName = Str("test?test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->nameHasForbiddenChars(forbiddenName));
    forbiddenName = Str("test\"test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->nameHasForbiddenChars(forbiddenName));
    forbiddenName = Str("test<test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->nameHasForbiddenChars(forbiddenName));
    forbiddenName = Str("test>test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->nameHasForbiddenChars(forbiddenName));
    forbiddenName = Str("test|test");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->nameHasForbiddenChars(forbiddenName));
    forbiddenName = Str("test\ntest");
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->nameHasForbiddenChars(forbiddenName));
    forbiddenName = Str("test ");
    CPPUNIT_ASSERT(!PlatformInconsistencyCheckerUtility::instance()->nameHasForbiddenChars(forbiddenName));
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::nameEndWithForbiddenSpace(forbiddenName));
#elif defined(KD_LINUX) && !defined(KD_MACOS)
    forbiddenName = std::string("test");
    forbiddenName.append(1, '\0');
    CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->nameHasForbiddenChars(forbiddenName));
#endif
}

void TestPlatformInconsistencyCheckerWorker::testCheckReservedNames() {
    SyncName allowedName = Str("....text");
    CPPUNIT_ASSERT(!PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(allowedName));

    std::array<SyncName, 2> reservedNames{{Str(".."), Str(".")}};
    for (const auto &name: reservedNames) {
        CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(name));
    }

#if defined(KD_WINDOWS)
    std::array<SyncName, 7> reservedWinNames{
            {Str("...."), Str("CON"), Str("LPT5"), Str("COM8"), Str("NUL"), Str("AUX"), Str("test.")}};

    for (const auto &name: reservedWinNames) {
        CPPUNIT_ASSERT(PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(name));
    }
#endif
}

void TestPlatformInconsistencyCheckerWorker::testNameClash() {
    const auto parentNode =
            std::make_shared<Node>(ReplicaSide::Remote, Str("parentNode"), NodeType::Directory, OperationType::Create, "parentID",
                                   0, 0, 12345, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());

    const auto nodeLower = std::make_shared<Node>(ReplicaSide::Remote, Str("a"), NodeType::File, OperationType::Create, "a", 0, 0,
                                                  12345, parentNode);
    const auto nodeUpper = std::make_shared<Node>(ReplicaSide::Remote, Str("A"), NodeType::File, OperationType::Create, "A", 0, 0,
                                                  12345, parentNode);

    CPPUNIT_ASSERT(parentNode->insertChildren(nodeLower));
    CPPUNIT_ASSERT(parentNode->insertChildren(nodeUpper));

    _syncPal->_platformInconsistencyCheckerWorker->checkNameClashAgainstSiblings(parentNode);

#if defined(KD_WINDOWS) || defined(KD_MACOS)
    CPPUNIT_ASSERT(!_syncPal->_platformInconsistencyCheckerWorker->_idsToBeRemoved.empty());
#else
    CPPUNIT_ASSERT(_syncPal->_platformInconsistencyCheckerWorker->_idsToBeRemoved.empty());
#endif
}

void TestPlatformInconsistencyCheckerWorker::testNameClashAfterRename() {
    // Create local files
    for (const std::vector<std::string> nodes = {{"a1"}, {"A"}}; const auto &n: nodes) {
        std::ofstream ofs(_tempDir.path() / n);
        ofs << "Some content.\n";
        ofs.close();
    }

    // Set up DB
    const DbNode dbNodeLower(2, _syncPal->syncDb()->rootNode().nodeId(), Str("a1"), Str("a1"), "la", "ra",
                             testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                             testhelpers::defaultFileSize, std::nullopt);
    const DbNode dbNodeUpper(3, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "lA", "rA", testhelpers::defaultTime,
                             testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                             testhelpers::defaultFileSize, std::nullopt);
    DbNodeId dbNodeIdLower;
    DbNodeId dbNodeIdUpper;
    bool constraintError = false;
    _syncPal->syncDb()->insertNode(dbNodeLower, dbNodeIdLower, constraintError);
    _syncPal->syncDb()->insertNode(dbNodeUpper, dbNodeIdUpper, constraintError);
    _syncPal->syncDb()->cache().reloadIfNeeded();
    // Set up remote tree
    const auto remoteParentNode = _syncPal->updateTree(ReplicaSide::Remote)->rootNode();
    const auto remoteNodeLower = std::make_shared<Node>(
            dbNodeIdLower, ReplicaSide::Remote, Str("a"), NodeType::File, OperationType::Move, "ra", 0, 0, 12345,
            _syncPal->updateTree(ReplicaSide::Remote)->rootNode(), Node::MoveOriginInfos("a1", remoteParentNode->id().value()));
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

#if defined(KD_WINDOWS) || defined(KD_MACOS)
    CPPUNIT_ASSERT(!_syncPal->_platformInconsistencyCheckerWorker->_idsToBeRemoved.empty());
#else
    CPPUNIT_ASSERT(_syncPal->_platformInconsistencyCheckerWorker->_idsToBeRemoved.empty());
#endif
}

void TestPlatformInconsistencyCheckerWorker::testExecute() {
    const auto parentNode =
            std::make_shared<Node>(ReplicaSide::Remote, Str("parentNode"), NodeType::Directory, OperationType::Create, "parentID",
                                   0, 0, 12345, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());

    const auto nodeLower = std::make_shared<Node>(ReplicaSide::Remote, Str("a"), NodeType::File, OperationType::Create, "a", 0, 0,
                                                  12345, parentNode);
    const auto nodeUpper = std::make_shared<Node>(ReplicaSide::Remote, Str("A"), NodeType::File, OperationType::Create, "A", 0, 0,
                                                  12345, parentNode);

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
    LOG_DEBUG(Log::instance()->getLogger(),
              "TestPlatformInconsistencyCheckerWorker::testExecute()"
              "Upper Node exists: "
                      << _syncPal->updateTree(ReplicaSide::Remote)->exists(*nodeUpper->id()));
    LOG_DEBUG(Log::instance()->getLogger(),
              "TestPlatformInconsistencyCheckerWorker::testExecute()"
              "Lower Node exists: "
                      << _syncPal->updateTree(ReplicaSide::Remote)->exists(*nodeLower->id()));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->exists(*parentNode->id()));

#if defined(KD_WINDOWS) || defined(KD_MACOS)
    CPPUNIT_ASSERT(exactly1exist);
#else
    (void) exactly1exist;
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->exists(*nodeUpper->id()) &&
                   _syncPal->updateTree(ReplicaSide::Remote)->exists(*nodeLower->id()));
#endif
}

void TestPlatformInconsistencyCheckerWorker::testNameSizeLocalTree() {
    initUpdateTree(ReplicaSide::Local);

    _syncPal->_platformInconsistencyCheckerWorker->execute();

    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->exists("testNode"));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->exists("aNode"));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->exists("ANode"));
    CPPUNIT_ASSERT(!_syncPal->updateTree(ReplicaSide::Local)->exists("longNameANode"));
    CPPUNIT_ASSERT(!_syncPal->updateTree(ReplicaSide::Local)->exists("testNode2"));
    CPPUNIT_ASSERT(!_syncPal->updateTree(ReplicaSide::Local)->exists("bNode"));
    CPPUNIT_ASSERT(!_syncPal->updateTree(ReplicaSide::Local)->exists("BNode"));
}

void TestPlatformInconsistencyCheckerWorker::testOnlySpaces() {
    CPPUNIT_ASSERT_EQUAL(true, PlatformInconsistencyCheckerUtility::isNameOnlySpaces(Str(" ")));
    CPPUNIT_ASSERT_EQUAL(true, PlatformInconsistencyCheckerUtility::isNameOnlySpaces(Str("     ")));
    CPPUNIT_ASSERT_EQUAL(false, PlatformInconsistencyCheckerUtility::isNameOnlySpaces(Str(" 1")));
    CPPUNIT_ASSERT_EQUAL(false, PlatformInconsistencyCheckerUtility::isNameOnlySpaces(Str("1 ")));
    CPPUNIT_ASSERT_EQUAL(false, PlatformInconsistencyCheckerUtility::isNameOnlySpaces(Str(" 1 ")));
}

void TestPlatformInconsistencyCheckerWorker::initUpdateTree(ReplicaSide side) {
    /* Initial tree structure:
     *  |
     *  +-- /test (dir) CREATE
     *  |   |
     *  |   +-- a.txt (file) CREATE
     *  |   +-- A.txt (file) CREATE
     *  |   +-- aaaaaaaaaaaaaaaaaaaa...aaaaaaaaaaaaaaaaaa.txt (file)  [maxNameLengh +1] CREATE
     *  |
     *  +-- /testDiraaaaaaaaaaaaaaa...aaaaaaaaaaaaaaaaa  (dir)  [maxNameLengh +1] MOVE
     *  |   |
     *  |   +-- b.txt (file) NONE
     *  |   +-- B.txt (file) NONE
     */

    const auto testNode =
            std::make_shared<Node>(_syncPal->updateTree(side)->side(), Str2SyncName("test"), NodeType::Directory,
                                   OperationType::Create, "testNode", 0, 0, 12345, _syncPal->updateTree(side)->rootNode());

    const auto aNode = std::make_shared<Node>(_syncPal->updateTree(side)->side(), Str2SyncName("a.txt"), NodeType::File,
                                              OperationType::Create, "aNode", 0, 0, 12345, testNode);

    const auto ANode = std::make_shared<Node>(_syncPal->updateTree(side)->side(), Str2SyncName("A.txt"), NodeType::File,
                                              OperationType::Create, "ANode", 0, 0, 12345, testNode);

    const auto longNameANode = std::make_shared<Node>(
            std::nullopt, _syncPal->updateTree(side)->side(),
            Str2SyncName("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                         "aaaaaaaaaaaa"
                         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.txt"),
            NodeType::File, OperationType::Create, "longNameANode", 0, 0, 12345, testNode);

    const auto testNode2 = std::make_shared<Node>(
            std::nullopt, _syncPal->updateTree(side)->side(),
            Str2SyncName("testDiraaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                         "aaaaaaaaaaaa"
                         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
            NodeType::Directory, OperationType::Move, "testNode2", 0, 0, 12345, _syncPal->updateTree(side)->rootNode(),
            Node::MoveOriginInfos(SyncPath("testDira"), _syncPal->updateTree(side)->rootNode()->id().value()));

    const auto bNode = std::make_shared<Node>(_syncPal->updateTree(side)->side(), Str2SyncName("b.txt"), NodeType::File,
                                              OperationType::None, "bNode", 0, 0, 12345, testNode2);

    const auto BNode = std::make_shared<Node>(_syncPal->updateTree(side)->side(), Str2SyncName("B.txt"), NodeType::File,
                                              OperationType::None, "BNode", 0, 0, 12345, testNode2);


    CPPUNIT_ASSERT(_syncPal->updateTree(side)->rootNode()->insertChildren(testNode));
    CPPUNIT_ASSERT(testNode->insertChildren(aNode));
    CPPUNIT_ASSERT(testNode->insertChildren(ANode));
    CPPUNIT_ASSERT(testNode->insertChildren(longNameANode));
    CPPUNIT_ASSERT(_syncPal->updateTree(side)->rootNode()->insertChildren(testNode2));
    CPPUNIT_ASSERT(testNode2->insertChildren(bNode));
    CPPUNIT_ASSERT(testNode2->insertChildren(BNode));

    _syncPal->updateTree(side)->insertNode(testNode);
    _syncPal->updateTree(side)->insertNode(aNode);
    _syncPal->updateTree(side)->insertNode(ANode);
    _syncPal->updateTree(side)->insertNode(longNameANode);
    _syncPal->updateTree(side)->insertNode(testNode2);
    _syncPal->updateTree(side)->insertNode(bNode);
    _syncPal->updateTree(side)->insertNode(BNode);
}

} // namespace KDC
