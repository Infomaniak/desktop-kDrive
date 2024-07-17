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
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);

    // Insert user, account, drive & sync
    User user(1, 1, "dummy");
    ParmsDb::instance()->insertUser(user);

    Account account(1, 1, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    Drive drive(1, 1, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);

    Sync sync(1, drive.dbId(), _tempDir.path, "");
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
    if (_syncPal && _syncPal->_syncDb) {
        _syncPal->_syncDb->close();
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
    std::shared_ptr<Node> parentNode =
        std::make_shared<Node>(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("parentNode"), NodeTypeDirectory,
                               OperationTypeCreate, "parentID", 0, 0, 12345, _syncPal->_remoteUpdateTree->rootNode());

    std::shared_ptr<Node> node_a = std::make_shared<Node>(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("a"),
                                                          NodeTypeFile, OperationTypeCreate, "a", 0, 0, 12345, parentNode);
    std::shared_ptr<Node> node_A = std::make_shared<Node>(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("A"),
                                                          NodeTypeFile, OperationTypeCreate, "A", 0, 0, 12345, parentNode);

    parentNode->insertChildren(node_a);
    parentNode->insertChildren(node_A);

    _syncPal->_platformInconsistencyCheckerWorker->checkNameClashAgainstSiblings(parentNode);

#if defined(WIN32) || defined(__APPLE__)
    CPPUNIT_ASSERT(!_syncPal->_platformInconsistencyCheckerWorker->_idsToBeRemoved.empty());
#else
    CPPUNIT_ASSERT(_syncPal->_platformInconsistencyCheckerWorker->_idsToBeRemoved.empty());
#endif
}

void TestPlatformInconsistencyCheckerWorker::testNameClashAfterRename() {
    // Create local files
    std::vector<std::string> nodes = {{"a1"}, {"A"}};
    for (const auto &n : nodes) {
        std::ofstream ofs(_tempDir.path / n);
        ofs << "Some content.\n";
        ofs.close();
    }

    // Set up DB
    DbNode dbNode_A(2, _syncPal->_syncDb->rootNode().nodeId(), Str("A"), Str("A"), "lA", "rA", defaultTime, defaultTime,
                    defaultTime, NodeType::NodeTypeDirectory, defaultSize, std::nullopt);
    DbNode dbNode_a(3, _syncPal->_syncDb->rootNode().nodeId(), Str("a1"), Str("a1"), "la", "ra", defaultTime, defaultTime,
                    defaultTime, NodeType::NodeTypeDirectory, defaultSize, std::nullopt);
    DbNodeId dbNodeIdDir_A;
    DbNodeId dbNodeIdDir_a;
    bool constraintError = false;
    _syncPal->_syncDb->insertNode(dbNode_A, dbNodeIdDir_A, constraintError);
    _syncPal->_syncDb->insertNode(dbNode_a, dbNodeIdDir_a, constraintError);

    // Set up remote tree
    std::shared_ptr<Node> remoteParentNode = _syncPal->_remoteUpdateTree->rootNode();
    std::shared_ptr<Node> node_ra =
        std::make_shared<Node>(dbNodeIdDir_a, _syncPal->_remoteUpdateTree->side(), Str("a"), NodeTypeFile, OperationTypeMove,
                               "ra", 0, 0, 12345, _syncPal->_remoteUpdateTree->rootNode());
    std::shared_ptr<Node> node_rA =
        std::make_shared<Node>(dbNodeIdDir_A, _syncPal->_remoteUpdateTree->side(), Str("A"), NodeTypeFile, OperationTypeNone,
                               "rA", 0, 0, 12345, _syncPal->_remoteUpdateTree->rootNode());

    remoteParentNode->insertChildren(node_ra);
    remoteParentNode->insertChildren(node_rA);

    _syncPal->_remoteUpdateTree->insertNode(node_ra);
    _syncPal->_remoteUpdateTree->insertNode(node_rA);

    // Set up local tree
    std::shared_ptr<Node> localParentNode = _syncPal->_localUpdateTree->rootNode();
    std::shared_ptr<Node> node_la = std::make_shared<Node>(dbNodeIdDir_a, _syncPal->_localUpdateTree->side(), Str("a1"),
                                                           NodeTypeFile, OperationTypeNone, "la", 0, 0, 12345, localParentNode);
    std::shared_ptr<Node> node_lA = std::make_shared<Node>(dbNodeIdDir_A, _syncPal->_localUpdateTree->side(), Str("A"),
                                                           NodeTypeFile, OperationTypeNone, "lA", 0, 0, 12345, localParentNode);

    localParentNode->insertChildren(node_la);
    localParentNode->insertChildren(node_lA);

    _syncPal->_localUpdateTree->insertNode(node_la);
    _syncPal->_localUpdateTree->insertNode(node_rA);

    // Check name clash
    _syncPal->_platformInconsistencyCheckerWorker->checkNameClashAgainstSiblings(remoteParentNode);

#if defined(WIN32) || defined(__APPLE__)
    CPPUNIT_ASSERT(!_syncPal->_platformInconsistencyCheckerWorker->_idsToBeRemoved.empty());
    CPPUNIT_ASSERT(!std::filesystem::exists(_tempDir.path / "a1"));
    std::error_code ec;
    auto dirIt = std::filesystem::recursive_directory_iterator(_syncPal->_localPath,
                                                               std::filesystem::directory_options::skip_permission_denied, ec);
    CPPUNIT_ASSERT(!ec);
    bool foundConflicted = false;
    for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
        if (Utility::startsWith(SyncName2Str(dirIt->path().filename()), std::string("a1"))) {
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
    std::shared_ptr<Node> parentNode =
        std::make_shared<Node>(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("parentNode"), NodeTypeDirectory,
                               OperationTypeCreate, "parentID", 0, 0, 12345, _syncPal->_remoteUpdateTree->rootNode());

    std::shared_ptr<Node> node_a = std::make_shared<Node>(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("a"),
                                                          NodeTypeFile, OperationTypeCreate, "a", 0, 0, 12345, parentNode);
    std::shared_ptr<Node> node_A = std::make_shared<Node>(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("A"),
                                                          NodeTypeFile, OperationTypeCreate, "A", 0, 0, 12345, parentNode);

    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(parentNode);
    parentNode->insertChildren(node_a);
    parentNode->insertChildren(node_A);

    _syncPal->_remoteUpdateTree->insertNode(parentNode);
    _syncPal->_remoteUpdateTree->insertNode(node_a);
    _syncPal->_remoteUpdateTree->insertNode(node_A);

    _syncPal->_platformInconsistencyCheckerWorker->execute();

    bool exactly1exist =
        (_syncPal->_remoteUpdateTree->exists(*node_A->id()) && !_syncPal->_remoteUpdateTree->exists(*node_a->id())) ||
        (!_syncPal->_remoteUpdateTree->exists(*node_A->id()) && _syncPal->_remoteUpdateTree->exists(*node_a->id()));

    CPPUNIT_ASSERT(_syncPal->_remoteUpdateTree->exists(*parentNode->id()));

#if defined(WIN32) || defined(__APPLE__)
    CPPUNIT_ASSERT(exactly1exist);
#else
    CPPUNIT_ASSERT(_syncPal->_remoteUpdateTree->exists(*node_A->id()) && _syncPal->_remoteUpdateTree->exists(*node_a->id()));
#endif
}

}  // namespace KDC
