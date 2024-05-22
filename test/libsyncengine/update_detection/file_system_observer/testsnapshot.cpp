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

#include "testsnapshot.h"

#include <memory>
#include "update_detection/file_system_observer/snapshot/snapshot.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommon/utility/utility.h"
#include "db/syncdb.h"
#include "db/parmsdb.h"

using namespace CppUnit;

namespace KDC {

void TestSnapshot::setUp() {
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
    bool alreadyExists;
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
}

void TestSnapshot::tearDown() {
    ParmsDb::instance()->close();
    if (_syncPal && _syncPal->_syncDb) {
        _syncPal->_syncDb->close();
    }
}

/**
 * Tree:
 *
 *            root
 *        _____|_____
 *       |          |
 *       A          B
 *       |
 *      AA
 *       |
 *      AAA
 */

void TestSnapshot::testSnapshot() {
    //    CPPUNIT_ASSERT(_syncPal->_localSnapshot->rootFolderId() == SyncDb::driveRootNode().nodeIdLocal());

    SnapshotItem itemA("a", SyncDb::driveRootNode().nodeIdLocal().value(), Str("A"), 1640995201, 1640995201,
                       NodeType::NodeTypeDirectory, 123);
    _syncPal->_localSnapshot->updateItem(itemA);
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->exists("a"));
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->name("a") == Str("A"));
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->type("a") == NodeType::NodeTypeDirectory);
    auto itItem = _syncPal->_localSnapshot->_items.find(SyncDb::driveRootNode().nodeIdLocal().value());
    CPPUNIT_ASSERT(itItem->second.childrenIds().find("a") != itItem->second.childrenIds().end());

    _syncPal->_localSnapshot->updateItem(SnapshotItem("a", SyncDb::driveRootNode().nodeIdLocal().value(), Str("A*"), 1640995202,
                                                      1640995202, NodeType::NodeTypeDirectory, 123));
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->name("a") == Str("A*"));

    SnapshotItem itemB("b", SyncDb::driveRootNode().nodeIdLocal().value(), Str("B"), 1640995203, 1640995203,
                       NodeType::NodeTypeDirectory, 123);
    _syncPal->_localSnapshot->updateItem(itemB);
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->exists("b"));
    itItem = _syncPal->_localSnapshot->_items.find(SyncDb::driveRootNode().nodeIdLocal().value());
    CPPUNIT_ASSERT(itItem->second.childrenIds().find("b") != itItem->second.childrenIds().end());

    SnapshotItem itemAA("aa", "a", Str("AA"), 1640995204, 1640995204, NodeType::NodeTypeDirectory, 123);
    _syncPal->_localSnapshot->updateItem(itemAA);
    itItem = _syncPal->_localSnapshot->_items.find("a");
    CPPUNIT_ASSERT(itItem->second.childrenIds().find("aa") != itItem->second.childrenIds().end());

    SnapshotItem itemAAA("aaa", "aa", Str("AAA"), 1640995205, 1640995205, NodeType::NodeTypeFile, 123);
    _syncPal->_localSnapshot->updateItem(itemAAA);
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->exists("aaa"));
    itItem = _syncPal->_localSnapshot->_items.find("aa");
    CPPUNIT_ASSERT(itItem->second.childrenIds().find("aaa") != itItem->second.childrenIds().end());

    SyncPath path;
    _syncPal->_localSnapshot->path("aaa", path);
    CPPUNIT_ASSERT(path == std::filesystem::path("A*/AA/AAA"));
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->name("aaa") == Str("AAA"));
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->lastModifed("aaa") == 1640995205);
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->type("aaa") == NodeType::NodeTypeFile);
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->contentChecksum("aaa") == "");  // Checksum never computed for now
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->itemId(std::filesystem::path("A*/AA/AAA")) == "aaa");

    _syncPal->_localSnapshot->updateItem(
        SnapshotItem("aa", "b", Str("AA"), 1640995204, 1640995204, NodeType::NodeTypeDirectory, 123));
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->parentId("aa") == "b");
    itItem = _syncPal->_localSnapshot->_items.find("b");
    CPPUNIT_ASSERT(itItem->second.childrenIds().find("aa") != itItem->second.childrenIds().end());
    itItem = _syncPal->_localSnapshot->_items.find("a");
    CPPUNIT_ASSERT(itItem->second.childrenIds().empty());

    _syncPal->_localSnapshot->removeItem("b");
    CPPUNIT_ASSERT(!_syncPal->_localSnapshot->exists("aaa"));
    CPPUNIT_ASSERT(!_syncPal->_localSnapshot->exists("aa"));
    CPPUNIT_ASSERT(!_syncPal->_localSnapshot->exists("b"));
    itItem = _syncPal->_localSnapshot->_items.find(SyncDb::driveRootNode().nodeIdLocal().value());
    CPPUNIT_ASSERT(itItem->second.childrenIds().find("b") == itItem->second.childrenIds().end());

    _syncPal->_localSnapshot->init();
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->_items.size() == 1);
}

}  // namespace KDC
