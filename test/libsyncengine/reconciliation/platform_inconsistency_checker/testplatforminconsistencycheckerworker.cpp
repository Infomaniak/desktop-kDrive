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
#include "libcommonserver/utility/utility.h"
#include "libcommon/utility/types.h"
#include "requests/parameterscache.h"

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
    _syncPal = std::shared_ptr<SyncPal>(new SyncPal(syncDbPath, "3.4.0", true));
    _syncPal->_syncDb->setAutoDelete(true);

    _syncPal->_platformInconsistencyCheckerWorker = std::shared_ptr<PlatformInconsistencyCheckerWorker>(
        new PlatformInconsistencyCheckerWorker(_syncPal, "Platform Inconsistency Checker", "PICH"));
}

void TestPlatformInconsistencyCheckerWorker::tearDown() {
    ParmsDb::instance()->close();
    _syncPal->_syncDb->close();
}

void TestPlatformInconsistencyCheckerWorker::testFixNameSize() {
    SyncName name =
        Str("SvhH49BbBbgx0quU24YNo7G1kXJzdORbQ3jfG20ZSrqWPtWavhbW37btXaK6ZKCzlr6N7sR6q6r7rjk0gbigETB4P8jFISnocNQc7IyiiwVZajnliVdc"
            "79sBUyZvV"
            "buz8qCw50y7LC7ESWcYZlDhUrmxn2heR5UEzyP25a3mw4Olq0WcBH5XvLxMC0qWbtHfveQYm3hw5Xc8gLWumb2VQNITwLEYzpNvnPzntrReXK8PSBCgh"
            "N7ujP9elhx1bWT");

    std::shared_ptr<Node> node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), name,
                                                                NodeTypeFile, OperationTypeNone, "0", 0, 0, 12345, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkNameSize(node->name())) {
        // TODO : to be re-written. Files with a name too long are not renamed anymore, they are blacklisted.
        //        // TODO : remove this, files are not renamed anymore
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(node->name().size() == MAX_NAME_LENGTH_WIN_SHORT);
    CPPUNIT_ASSERT(node->name().empty());

    name =
        Str("46fpmz5XC2fBH4Rgv8eqdEUMAKtgo5as2fe2pq8t7o8rxvJ2zeKpDuNoyVxN4mIdKx0SViroqO31oOwhz5ZGXimXJWGYVwefihprlPl6bKCcbEjuVv5L"
            "qFsUXZ5NMkICIqMjrIjFHmPl9W1B"
            "xywNr1W6Fjy3RnHVYJxTho5PJlc3zh0bnPwBEUqT6wtuOm5Iz5BQNBaJfbQ40HWulNZ5b9uk1wHHuLoYXIg9TpH49K4U68tsr16NjWzjBZ7pU3aV5XDc"
            "LokPLaCGNfwBXYVzEbw4xnGeQQHSoSNgAK3w3GoS");

    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), name, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 12345, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkNameSize(node->name())) {
        // TODO : to be re-written. Files with a name too long are not renamed anymore, they are blacklisted.
        //        // TODO : remove this, files are not renamed anymore
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(node->name().size() > MAX_NAME_LENGTH_WIN_SHORT);
    CPPUNIT_ASSERT(node->name().size() == MAX_NAME_LENGTH_WIN_SHORT);
}

void TestPlatformInconsistencyCheckerWorker::testFixNameForbiddenChars() {
    SyncName name = Str("a/b:c/d:e:");
    std::shared_ptr<Node> node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), name,
                                                                NodeTypeFile, OperationTypeNone, "0", 0, 0, 12345, nullptr));
    SyncName newvalidLocalName;
    if (PlatformInconsistencyCheckerUtility::instance()->fixNameForbiddenChars(node->name(), newvalidLocalName)) {
        // TODO : to be re-written. Files with forbidden characters in their name are not renamed anymore, they are blacklisted.
        //        node->setname(newvalidLocalName);
    }
    CPPUNIT_ASSERT(node->name().find_first_of(Str2SyncName(FORBIDDEN_FILENAME_CHARS)) == std::string::npos);

#if defined(WIN32) || defined(__APPLE__)
    CPPUNIT_ASSERT(node->name() == Str("a%2fb%3ac%2fd%3ae%3a"));
#else
    CPPUNIT_ASSERT(node->name() == Str("a%2fb:c%2fd:e:"));
#endif

    name = Str("bcd:ef:");
    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), name, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 12345, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->fixNameForbiddenChars(node->name(), newvalidLocalName)) {
        // TODO : to be re-written. Files with forbidden characters in their name are not renamed anymore, they are blacklisted.
        //        node->setname(newvalidLocalName);
    }
    CPPUNIT_ASSERT(node->name().find_first_of(Str2SyncName(FORBIDDEN_FILENAME_CHARS)) == std::string::npos);

#if defined(WIN32) || defined(__APPLE__)
    CPPUNIT_ASSERT(node->name() == Str("bcd%3aef%3a"));
#else
    CPPUNIT_ASSERT(node->name() == Str(""));
#endif

    name = Str("/b:/");
    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), name, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 12345, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->fixNameForbiddenChars(node->name(), newvalidLocalName)) {
        // TODO : to be re-written. Files with forbidden characters in their name are not renamed anymore, they are blacklisted.
        //        node->setname(newvalidLocalName);
    }
    CPPUNIT_ASSERT(node->name().find_first_of(Str2SyncName(FORBIDDEN_FILENAME_CHARS)) == std::string::npos);

#if defined(WIN32) || defined(__APPLE__)
    CPPUNIT_ASSERT(node->name() == Str("%2fb%3a%2f"));
#else
    CPPUNIT_ASSERT(node->name() == Str("%2fb:%2f"));
#endif

    name = Str("testKdrive");
    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), name, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 12345, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->fixNameForbiddenChars(node->name(), newvalidLocalName)) {
        // TODO : to be re-written. Files with forbidden characters in their name are not renamed anymore, they are blacklisted.
        //        node->setname(newvalidLocalName);
    }
    CPPUNIT_ASSERT(node->name().find_first_of(Str2SyncName(FORBIDDEN_FILENAME_CHARS)) == std::string::npos);
    CPPUNIT_ASSERT(node->name() == Str(""));
}

void TestPlatformInconsistencyCheckerWorker::testFixReservedNames() {
    std::shared_ptr<Node> node = nullptr;

    SyncName dots = Str("....");
    SyncName dotsText = Str("....text");
    SyncName dot2 = Str("..");
    SyncName dot1 = Str(".");

    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), dots, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 12345, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        // TODO : remove this, files are not renamed anymore
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(node->name() == dots && node->name().empty());

    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), dotsText, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 12345, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        // TODO : remove this, files are not renamed anymore
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(node->name() == dotsText && node->name().empty());

    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), dot2, NodeTypeDirectory,
                                          OperationTypeNone, "0", 0, 0, 12345, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        // TODO : remove this, files are not renamed anymore
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(node->name() == dot2 && !node->name().empty());

    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), dot1, NodeTypeDirectory,
                                          OperationTypeNone, "0", 0, 0, 12345, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        // TODO : remove this, files are not renamed anymore
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(node->name() == dot1 && !node->name().empty());

    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), dot2, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 12345, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        // TODO : remove this, files are not renamed anymore
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(node->name() == dot2 && !node->name().empty());

    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), dot1, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 12345, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        // TODO : remove this, files are not renamed anymore
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(node->name() == dot1 && !node->name().empty());

#if defined(WIN32)
    SyncName con = L"CON";
    SyncName lpt11 = L"LPT11";
    SyncName com8 = L"COM8";
    SyncName lpt11ext = L"LPT11.extt";
    SyncName lpt6ext = L"LPT6.ext";
    SyncName nul = L"NUL.eee";
    SyncName aux6 = L"AUX6";
    SyncName com1dot = L"COM1.";
    SyncName prndot = L"PRN.";
    SyncName com77dot = L"COM77.";

    node = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), con, NodeTypeFile, OperationTypeNone, "0", 0, 0, 0, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(!node->name().empty());

    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), lpt11, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 0, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(node->name().empty());

    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), com8, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 0, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(!node->name().empty());

    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), lpt11ext, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 0, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(node->name().empty());

    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), lpt6ext, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 0, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(!node->name().empty());

    node = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), nul, NodeTypeFile, OperationTypeNone, "0", 0, 0, 0, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(!node->name().empty());

    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), aux6, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 0, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(node->name().empty());

    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), com1dot, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 0, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(!node->name().empty());

    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), prndot, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 0, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(!node->name().empty());

    node = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), com77dot, NodeTypeFile,
                                          OperationTypeNone, "0", 0, 0, 0, nullptr));
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(node->name())) {
        // TODO : to be re-written. Files with reserved names are not renamed anymore, they are blacklisted.
        //        node->setname(PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        //            node->name(), PlatformInconsistencyCheckerUtility::SuffixTypeRename));
    }
    CPPUNIT_ASSERT(!node->name().empty());
#endif
}

void TestPlatformInconsistencyCheckerWorker::testNameClash_noExtension() {
    SyncName name_a = Str("a");
    SyncName name_A = Str("A");

    std::shared_ptr<Node> node_a =
        std::make_shared<Node>(std::nullopt, _syncPal->_remoteUpdateTree->side(), name_a, NodeTypeFile, OperationTypeCreate, "1",
                               0, 0, 12345, _syncPal->_remoteUpdateTree->rootNode());
    std::shared_ptr<Node> node_A =
        std::make_shared<Node>(std::nullopt, _syncPal->_remoteUpdateTree->side(), name_A, NodeTypeFile, OperationTypeCreate, "2",
                               0, 0, 12345, _syncPal->_remoteUpdateTree->rootNode());

    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(node_a);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(node_A);

    _syncPal->_remoteUpdateTree->insertNode(node_a);
    _syncPal->_remoteUpdateTree->insertNode(node_A);

    _syncPal->_platformInconsistencyCheckerWorker->execute();

    CPPUNIT_ASSERT(node_a->name().empty() != node_a->name().empty());
}

void TestPlatformInconsistencyCheckerWorker::testNameClash_withExtension() {
    SyncName name_a = Str("a.jpg");
    SyncName name_A = Str("A.jpg");

    std::shared_ptr<Node> node_a =
        std::make_shared<Node>(std::nullopt, _syncPal->_remoteUpdateTree->side(), name_a, NodeTypeFile, OperationTypeCreate, "1",
                               0, 0, 12345, _syncPal->_remoteUpdateTree->rootNode());
    std::shared_ptr<Node> node_A =
        std::make_shared<Node>(std::nullopt, _syncPal->_remoteUpdateTree->side(), name_A, NodeTypeFile, OperationTypeCreate, "2",
                               0, 0, 12345, _syncPal->_remoteUpdateTree->rootNode());

    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(node_a);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(node_A);

    _syncPal->_remoteUpdateTree->insertNode(node_a);
    _syncPal->_remoteUpdateTree->insertNode(node_A);

    _syncPal->_platformInconsistencyCheckerWorker->execute();

    CPPUNIT_ASSERT(node_a->name().empty() != node_a->name().empty());
}

void TestPlatformInconsistencyCheckerWorker::testNameClash_extensionFinishWithDot() {
    SyncName name_a = Str("a.");
    SyncName name_A = Str("A.");

    std::shared_ptr<Node> node_a =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), name_a, NodeTypeFile,
                                       OperationTypeCreate, "1", 0, 0, 12345, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> node_A =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), name_A, NodeTypeFile,
                                       OperationTypeCreate, "2", 0, 0, 12345, _syncPal->_remoteUpdateTree->rootNode()));

    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(node_a);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(node_A);

    _syncPal->_remoteUpdateTree->insertNode(node_a);
    _syncPal->_remoteUpdateTree->insertNode(node_A);

    _syncPal->_platformInconsistencyCheckerWorker->execute();

    CPPUNIT_ASSERT(node_a->name().empty() != node_a->name().empty());
}

void TestPlatformInconsistencyCheckerWorker::testExecute() {
    SyncName name =
        Str("cSvhH49BbBbgx0quU24YNo7G1kXJzdORbQ3jfG20ZSrqWPtWavhbW37btXaK6ZKCzlr6N7sR6q6r7rjk0gbigETB4P8jFISnocNQc7IyiiwVZajnliVd"
            "c79sBUyZvV"
            "buz8qCw50y7LC7ESWcYZlDhUrmxn2heR5UEzyP25a3mw4Olq0WcBH5XvLxMC0qWbtHfveQYm3hw5Xc8gLWumb2VQNITwLEYzpNvnPzntrReXK8PSBCgh"
            "N7ujP9elhx1bWT");
    SyncName namespecial =
        Str("cncvvrlfgfjhyeneir/pf/fvqz/dzdjs:n:uprjnofbkjivbsrzguxiemlpgofv:at:naaygamdtqfudwomtn/lozplyxdtmgdqxtfvgdwpffm:jea/"
            "adbw:/:zb"
            "kpzcccavngwaqekl:ffcwegri/:oxocgvgxgwyhirv:hgzy/bgssftd/bsucnymrb:kluvklkwxfzzcmhp/"
            "kqtpzhufctqvr:nyzyonhrhrqqrjvwxlexxbwlkyoecaxg:dk");

    std::shared_ptr<Node> node1 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("Dir:1"), NodeTypeDirectory,
                                       OperationTypeNone, "1", 0, 0, 12345, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> node2 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("D/ir 2/"), NodeTypeDirectory,
                                       OperationTypeNone, "2", 0, 0, 12345, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> node3 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("Dir 3"), NodeTypeDirectory,
                                       OperationTypeNone, "3", 0, 0, 12345, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> node4 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("Dir 4"), NodeTypeDirectory,
                                       OperationTypeNone, "4", 0, 0, 12345, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> node11 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                       OperationTypeNone, "11", 0, 0, 12345, node1));
    std::shared_ptr<Node> node111 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("Dir 1.1.1"), NodeTypeDirectory,
                                       OperationTypeNone, "111", 0, 0, 12345, node11));
    std::shared_ptr<Node> node1111 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("File 1.1::/.1.1"), NodeTypeFile,
                                       OperationTypeNone, "1111", 0, 0, 12345, node111));
    std::shared_ptr<Node> node31 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("Dir 3.1"), NodeTypeDirectory,
                                       OperationTypeNone, "31", 0, 0, 12345, node3));
    std::shared_ptr<Node> node41 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), namespecial, NodeTypeDirectory,
                                       OperationTypeNone, "41", 0, 0, 12345, node4));
    std::shared_ptr<Node> node411bis =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("dIR//::4.1.1😀"),
                                       NodeTypeDirectory, OperationTypeNone, "411bis", 0, 0, 12345, node41));
    std::shared_ptr<Node> node411 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("Dir//::4.1.1😀"),
                                       NodeTypeDirectory, OperationTypeNone, "411", 0, 0, 12345, node41));
    std::shared_ptr<Node> node4111 = std::shared_ptr<Node>(new Node(
        std::nullopt, _syncPal->_remoteUpdateTree->side(), name, NodeTypeFile, OperationTypeNone, "4111", 0, 0, 12345, node411));

    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(node1);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(node2);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(node3);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(node4);
    node1->insertChildren(node11);
    node11->insertChildren(node111);
    node111->insertChildren(node1111);
    node3->insertChildren(node31);
    node4->insertChildren(node41);
    node41->insertChildren(node411);
    node41->insertChildren(node411bis);
    node411->insertChildren(node4111);

    _syncPal->_remoteUpdateTree->insertNode(node1111);
    _syncPal->_remoteUpdateTree->insertNode(node111);
    _syncPal->_remoteUpdateTree->insertNode(node11);
    _syncPal->_remoteUpdateTree->insertNode(node1);
    _syncPal->_remoteUpdateTree->insertNode(node2);
    _syncPal->_remoteUpdateTree->insertNode(node3);
    _syncPal->_remoteUpdateTree->insertNode(node4);
    _syncPal->_remoteUpdateTree->insertNode(node31);
    _syncPal->_remoteUpdateTree->insertNode(node41);
    _syncPal->_remoteUpdateTree->insertNode(node411);
    _syncPal->_remoteUpdateTree->insertNode(node411bis);
    _syncPal->_remoteUpdateTree->insertNode(node4111);

    _syncPal->_platformInconsistencyCheckerWorker->execute();

    // win & apple doesn't handle ":"
#if defined(WIN32) || defined(__APPLE__)
    CPPUNIT_ASSERT(!_syncPal->_remoteUpdateTree->nodes()["41"]->name().empty());

    // Only one node should have been renamed between 411 and 411bis
    CPPUNIT_ASSERT(!_syncPal->_remoteUpdateTree->nodes()["411bis"]->name().empty());
    CPPUNIT_ASSERT(!_syncPal->_remoteUpdateTree->nodes()["411bis"]->name().empty());

    CPPUNIT_ASSERT(!_syncPal->_remoteUpdateTree->nodes()["1"]->name().empty());
    CPPUNIT_ASSERT(!_syncPal->_remoteUpdateTree->nodes()["1111"]->name().empty());
#else
    // special case because linux accept ':' in filesys names
    // Only one node should have been renamed between 411 and 411bis
    CPPUNIT_ASSERT(!_syncPal->_remoteUpdateTree->nodes()["411bis"]->name().empty());
    CPPUNIT_ASSERT(!_syncPal->_remoteUpdateTree->nodes()["411"]->name().empty());

    CPPUNIT_ASSERT(_syncPal->_remoteUpdateTree->nodes()["1111"]->name() == Str("File 1.1::%2f.1.1"));
    CPPUNIT_ASSERT(!_syncPal->_remoteUpdateTree->nodes()["41"]->name().empty());
#endif
    CPPUNIT_ASSERT(_syncPal->_remoteUpdateTree->nodes()["2"]->name() == Str("D%2fir 2%2f"));

    if (PlatformInconsistencyCheckerUtility::_maxPathLength > MAX_PATH_LENGTH_WIN_SHORT) {
        // child of node "41" haven't been treated because the path size was 261.
        CPPUNIT_ASSERT(_syncPal->_remoteUpdateTree->nodes()["4111"]->name().size() == MAX_NAME_LENGTH_WIN_SHORT);
    }

    CPPUNIT_ASSERT(_syncPal->_remoteUpdateTree->nodes()["41"]->name().size() == MAX_NAME_LENGTH_WIN_SHORT);
}

}  // namespace KDC
