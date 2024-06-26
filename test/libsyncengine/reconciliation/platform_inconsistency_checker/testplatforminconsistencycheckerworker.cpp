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
    // Create SyncPal
    bool alreadyExists;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParmsDb::instance()->setAutoDelete(true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    std::filesystem::remove(syncDbPath);
    _syncPal = std::make_shared<SyncPal>(syncDbPath, "3.4.0", true);
    _syncPal->_syncDb->setAutoDelete(true);
    _syncPal->_tmpBlacklistManager = std::make_shared<TmpBlacklistManager>(_syncPal);

    _syncPal->_platformInconsistencyCheckerWorker =
        std::make_shared<PlatformInconsistencyCheckerWorker>(_syncPal, "Platform Inconsistency Checker", "PICH");
}

void TestPlatformInconsistencyCheckerWorker::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::instance().reset();
    _syncPal->_syncDb->close();
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

#if defined(WIN32) || defined(__APPLE__)
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
#else
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
