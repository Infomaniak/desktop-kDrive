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

#include "testintegration.h"

#include <memory>

#include "db/db.h"
#include "jobs/local/localcopyjob.h"
#include "jobs/local/localdeletejob.h"
#include "jobs/local/localmovejob.h"
#include "jobs/jobmanager.h"
#include "requests/syncnodecache.h"
#include "requests/exclusiontemplatecache.h"
#include "libcommon/utility/utility.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/network/proxy.h"
#include "libsyncengine/jobs/network/API_v2/copytodirectoryjob.h"
#include "libsyncengine/jobs/network/API_v2/createdirjob.h"
#include "libsyncengine/jobs/network/API_v2/deletejob.h"
#include "libsyncengine/jobs/network/API_v2/duplicatejob.h"
#include "libsyncengine/jobs/network/API_v2/getfileinfojob.h"
#include "libsyncengine/jobs/network/API_v2/getfilelistjob.h"
#include "libsyncengine/jobs/network/API_v2/movejob.h"
#include "libsyncengine/jobs/network/API_v2/renamejob.h"
#include "libsyncengine/jobs/network/API_v2/upload/uploadjob.h"
#include "libsyncengine/update_detection/file_system_observer/filesystemobserverworker.h"
#include "requests/syncnodecache.h"
#include "requests/exclusiontemplatecache.h"
#include "libcommon/utility/utility.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/network/proxy.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "test_utility/testhelpers.h"
#include "requests/parameterscache.h"
#include "test_utility/remotetemporarydirectory.h"

using namespace CppUnit;

namespace KDC {

// Temporary test in drive "kDrive Desktop Team"
const std::string testExecutorFolderRemoteId = "66080";
const std::string testExecutorSubFolderRemoteId = "66081";
const std::string testExecutorFileRemoteId = "66082";
const std::string testExecutorFileCopyRemoteId = "66083";
const SyncName testExecutorFolderName = Str("test_executor");
const SyncName testExecutorSubFolderName = Str("test_executor_sub");
const SyncPath testExecutorFolderRelativePath = Str("Common documents/test/test_executor");

const std::string testInconsistencyFileRemoteId = "60765";
const std::string testLongFileRemoteId = "19146";

const std::string test_beaucoupRemoteId = "24642";
const NodeId test_commonDocumentsNodeId = "3";

void TestIntegration::setUp() {
    TestBase::start();
    _logger = Log::instance()->getLogger();

    LOGW_DEBUG(_logger, L"$$$$$ Set Up");

    const testhelpers::TestVariables testVariables;

    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    bool alreadyExists;
    std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync
    int userId(12321);
    User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    _localPath = _localTmpDir.path();
    _remotePath = testVariables.remotePath;
    NodeId localNodeId;
    CPPUNIT_ASSERT(IoHelper::getNodeId(_localPath, localNodeId));
    Sync sync(1, drive.dbId(), _localPath, localNodeId, _remotePath);
    (void) ParmsDb::instance()->insertSync(sync);

    // Setup proxy
    Parameters parameters;
    bool found;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    _syncPal = std::make_shared<SyncPal>(std::make_shared<VfsOff>(VfsSetupParams(Log::instance()->getLogger())), sync.dbId(),
                                         KDRIVE_VERSION_STRING);
    _syncPal->createSharedObjects();
    _syncPal->syncDb()->setAutoDelete(true);
    ParametersCache::instance()->parameters().setExtendedLog(true); // Enable extended log to see more details in the logs

    // Insert items to blacklist
    SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::BlackList, {test_beaucoupRemoteId});

    // Insert items to excluded templates in DB
    std::vector<ExclusionTemplate> templateVec = {
            ExclusionTemplate(".DS_Store", true) // TODO : to be removed once we have a default list of file excluded implemented
            ,
            ExclusionTemplate("*_conflict_*_*_*",
                              true) // TODO : to be removed once we have a default list of file excluded implemented
            ,
            ExclusionTemplate("*_excluded", true)};
    ExclusionTemplateCache::instance()->update(true, templateVec);

    _testFctPtrVector = {
            &TestIntegration::testCreateLocal,
            &TestIntegration::testEditLocal,
            &TestIntegration::testMoveLocal,
            &TestIntegration::testRenameLocal,
            &TestIntegration::testDeleteLocal,
            &TestIntegration::testCreateRemote,
            &TestIntegration::testEditRemote,
            &TestIntegration::testMoveRemote,
            &TestIntegration::testRenameRemote,
            &TestIntegration::testDeleteRemote,
            &TestIntegration::testSimultaneousChanges,
            &TestIntegration::testInconsistency,
            &TestIntegration::testCreateCreatePseudoConflict,
            &TestIntegration::testCreateCreateConflict,
            &TestIntegration::testEditEditPseudoConflict,
            &TestIntegration::testEditEditConflict,
            &TestIntegration::testMoveCreateConflict,
            &TestIntegration::testEditDeleteConflict1,
            &TestIntegration::testEditDeleteConflict2,
            &TestIntegration::testMoveDeleteConflict1,
            &TestIntegration::testMoveDeleteConflict2,
            &TestIntegration::testMoveDeleteConflict3,
            &TestIntegration::testMoveDeleteConflict4,
            &TestIntegration::testMoveDeleteConflict5,
            &TestIntegration::testMoveParentDeleteConflict,
            &TestIntegration::testCreateParentDeleteConflict,
            &TestIntegration::testMoveMoveSourcePseudoConflict,
            &TestIntegration::testMoveMoveSourceConflict,
            &TestIntegration::testMoveMoveDestConflict,
            &TestIntegration::testMoveMoveCycleConflict,
    };
}

void TestIntegration::tearDown() {
    _syncPal->stop(false, true, false);
    ParmsDb::instance()->close();
    ParmsDb::reset();
    JobManager::instance()->stop();
    JobManager::instance()->clear();
    TestBase::stop();
}

void TestIntegration::testAll() {
    // Start sync
    _syncPal->start();
    Utility::msleep(1000);
    CPPUNIT_ASSERT(_syncPal->isRunning());

    // Wait for end of 1st sync
    while (!_syncPal->syncHasFullyCompleted()) {
        Utility::msleep(1000);
    }

    // Run test cases
    for (uint i = 0; i < _testFctPtrVector.size(); i++) {
        (this->*_testFctPtrVector[i])();
    }
}

void TestIntegration::testCreateLocal() {
    LOGW_DEBUG(_logger, L"$$$$$ test create local file");
    std::cout << "test create local file : " << std::endl;

    // Simulate a create by duplicating an existing file
    SyncPath localExecutorFolderPath = _localPath / testExecutorFolderRelativePath;
    SyncPath testFile = localExecutorFolderPath / "test_executor.txt";
    SyncPath newFileName = Str("testLocal_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");
    _newTestFilePath = localExecutorFolderPath / newFileName;
    CPPUNIT_ASSERT(std::filesystem::copy_file(testFile, _newTestFilePath));

    waitForSyncToFinish(SourceLocation::currentLoc());

    GetFileListJob job(_driveDbId, testExecutorFolderRemoteId);
    job.runSynchronously();

    Poco::JSON::Object::Ptr resObj = job.jsonRes();
    CPPUNIT_ASSERT(resObj);

    bool newFileFound = false;
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    CPPUNIT_ASSERT(dataArray);

    for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
        Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
        if (newFileName == obj->get(nameKey).toString()) {
            newFileFound = true;
            _newTestFileRemoteId = obj->get(idKey).toString();
            break;
        }
    }
    CPPUNIT_ASSERT(newFileFound);

    std::cout << "OK" << std::endl;
}

void TestIntegration::testEditLocal() {
    LOGW_DEBUG(_logger, L"$$$$$ test edit local file");
    std::cout << "test edit local file : ";

    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(_newTestFilePath, &fileStat, exists);
    _newTestFileLocalId = std::to_string(fileStat.inode);

    SyncTime prevModTime = _syncPal->_remoteSnapshot->lastModified(_newTestFileRemoteId);
    SyncName testCallStr = Str(R"(echo "This is an edit test )") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) +
                           Str(R"(" >> ")") + _newTestFilePath.native().c_str() + Str(R"(")");
    std::system(SyncName2Str(testCallStr).c_str());

    waitForSyncToFinish(SourceLocation::currentLoc());

    SyncTime newModTime = _syncPal->_remoteSnapshot->lastModified(_newTestFileRemoteId);

    CPPUNIT_ASSERT(newModTime > prevModTime);

    std::cout << "OK" << std::endl;
}

void TestIntegration::testMoveLocal() {
    LOGW_DEBUG(_logger, L"$$$$$ test move local file");
    std::cout << "test move local file : ";

    std::filesystem::path localTestDirPath = _localPath / testExecutorFolderRelativePath / testExecutorSubFolderName;
    std::filesystem::path destinationPath = localTestDirPath / _newTestFilePath.filename();

    SyncName testCallStr;
#ifdef _WIN32
    testCallStr = Str(R"(move ")") + _newTestFilePath.make_preferred().native() + Str(R"(" ")") +
                  destinationPath.make_preferred().native() + Str(R"(")");
#else
    testCallStr = Str(R"(mv ")") + _newTestFilePath.make_preferred().native() + Str(R"(" ")") +
                  destinationPath.make_preferred().native() + Str(R"(")");
#endif
    std::system(SyncName2Str(testCallStr).c_str());

    waitForSyncToFinish(SourceLocation::currentLoc());

    NodeId parentId = _syncPal->_remoteSnapshot->parentId(_newTestFileRemoteId);
    CPPUNIT_ASSERT(parentId == testExecutorSubFolderRemoteId);

    _newTestFilePath = destinationPath;

    std::cout << "OK" << std::endl;
}

void TestIntegration::testRenameLocal() {
    LOGW_DEBUG(_logger, L"$$$$$ test rename local file");
    std::cout << "test rename local file : ";

    std::string newName = _newTestFilePath.stem().string() + "_renamed" + _newTestFilePath.extension().string();
    std::filesystem::path destinationPath = _newTestFilePath.parent_path() / newName;

    SyncName testCallStr;
#ifdef _WIN32
    testCallStr = Str(R"(ren ")") + _newTestFilePath.make_preferred().native() + Str(R"(" ")") +
                  destinationPath.make_preferred().native() + Str(R"(")");
#else
    testCallStr = Str(R"(mv ")") + _newTestFilePath.make_preferred().native() + Str(R"(" ")") +
                  destinationPath.make_preferred().native() + Str(R"(")");
#endif
    std::system(SyncName2Str(testCallStr).c_str());

    waitForSyncToFinish(SourceLocation::currentLoc());

    SyncName remoteName = _syncPal->_remoteSnapshot->name(_newTestFileRemoteId);
    SyncName::size_type n = remoteName.find(Str("_renamed"));
    CPPUNIT_ASSERT(n != std::string::npos);

    _newTestFilePath = destinationPath;

    std::cout << "OK" << std::endl;
}

void TestIntegration::testDeleteLocal() {
    LOGW_DEBUG(_logger, L"$$$$$ test delete local file");
    std::cout << "test delete local file : ";

    SyncName testCallStr;
#ifdef _WIN32
    testCallStr = Str(R"(del ")") + _newTestFilePath.make_preferred().native() + Str(R"(")");
#else
    testCallStr = Str(R"(rm -r ")") + _newTestFilePath.make_preferred().native() + Str(R"(")");
#endif
    std::system(SyncName2Str(testCallStr).c_str());

    waitForSyncToFinish(SourceLocation::currentLoc());

    CPPUNIT_ASSERT(!_syncPal->_localSnapshot->exists(_newTestFileLocalId));

    std::cout << "OK" << std::endl;
}

void TestIntegration::testCreateRemote() {
    LOGW_DEBUG(_logger, L"$$$$$ test create remote file");
    std::cout << "test create remote file : ";

    // Simulate a create by duplicating an existing file
    _newTestFilePath = Str("testRemote_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");
    DuplicateJob job(_syncPal->vfs(), _driveDbId, testExecutorFileRemoteId, _newTestFilePath.native());
    job.runSynchronously();
    _newTestFileRemoteId = job.nodeId();

    waitForSyncToFinish(SourceLocation::currentLoc());

    bool found = false;
    CPPUNIT_ASSERT(
            _syncPal->syncDb()->correspondingNodeId(ReplicaSide::Remote, _newTestFileRemoteId, _newTestFileLocalId, found));
    CPPUNIT_ASSERT(found);
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->exists(_newTestFileLocalId));

    std::cout << "OK" << std::endl;
}

void TestIntegration::testEditRemote() {
    LOGW_DEBUG(_logger, L"$$$$$ test edit remote file");
    std::cout << "test edit remote file : ";

    SyncTime prevModTime = _syncPal->_localSnapshot->lastModified(_newTestFileLocalId);

    std::filesystem::path tmpFile = std::filesystem::temp_directory_path() / "tmp_test_file.txt";
    SyncName testCallStr = Str(R"(echo "This is an edit test )") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) +
                           Str(R"(" >> ")") + tmpFile.make_preferred().native() + Str(R"(")");
    std::system(SyncName2Str(testCallStr).c_str());

    UploadJob setupUploadJob(_syncPal->vfs(), _driveDbId, tmpFile, _newTestFilePath.filename().native(),
                             testExecutorFolderRemoteId, 0, 0);
    setupUploadJob.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    bool found = false;
    CPPUNIT_ASSERT(_syncPal->syncDb()->correspondingNodeId(ReplicaSide::Remote, _newTestFileRemoteId, _newTestFileLocalId,
                                                           found)); // Update the local ID
    CPPUNIT_ASSERT(found);
    SyncTime newModTime = _syncPal->_localSnapshot->lastModified(_newTestFileLocalId);
    CPPUNIT_ASSERT(newModTime > prevModTime);

    std::cout << "OK" << std::endl;
}

void TestIntegration::testMoveRemote() {
    LOGW_DEBUG(_logger, L"$$$$$ test move remote file");
    std::cout << "test move remote file : ";

    NodeId parentId = _syncPal->_localSnapshot->parentId(_newTestFileLocalId);
    SyncName parentName = _syncPal->_localSnapshot->name(parentId);
    CPPUNIT_ASSERT(parentName == testExecutorFolderName);

    MoveJob job(_syncPal->vfs(), _driveDbId, "", _newTestFileRemoteId, testExecutorSubFolderRemoteId);
    job.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    parentId = _syncPal->_localSnapshot->parentId(_newTestFileLocalId);
    parentName = _syncPal->_localSnapshot->name(parentId);
    CPPUNIT_ASSERT(parentName == testExecutorSubFolderName);

    std::cout << "OK" << std::endl;
}

void TestIntegration::testRenameRemote() {
    LOGW_DEBUG(_logger, L"$$$$$ test rename remote file");
    std::cout << "test rename remote file : ";

    std::string newName = _newTestFilePath.stem().string() + "_renamed" + _newTestFilePath.extension().string();

    RenameJob job(_syncPal->vfs(), _driveDbId, _newTestFileRemoteId, Str2SyncName(newName));
    job.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    SyncName localName = _syncPal->_localSnapshot->name(_newTestFileLocalId);
    SyncName::size_type n = localName.find(Str("_renamed"));
    CPPUNIT_ASSERT(n != std::string::npos);

    std::cout << "OK" << std::endl;
}

void TestIntegration::testDeleteRemote() {
    LOGW_DEBUG(_logger, L"$$$$$ test delete remote file");
    std::cout << "test delete remote file : ";

    DeleteJob job(_driveDbId, _newTestFileRemoteId, "", "",
                  NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    job.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    CPPUNIT_ASSERT(!_syncPal->_localSnapshot->exists(_newTestFileLocalId));

    std::cout << "OK" << std::endl;
}

void TestIntegration::testSimultaneousChanges() {
    LOGW_DEBUG(_logger, L"$$$$$ test simultaneous changes");
    std::cout << "test simultaneous changes : ";

    // Simulate local file edition
    std::filesystem::path localFilePath = _localPath / testExecutorFolderRelativePath / "test_executor_copy.txt";

    SyncTime prevModTime = _syncPal->_remoteSnapshot->lastModified(testExecutorFileCopyRemoteId);
    SyncName testCallStr = Str(R"(echo "This is an edit test )") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) +
                           Str(R"(" >> ")") + localFilePath.make_preferred().native() + Str(R"(")");
    std::system(SyncName2Str(testCallStr).c_str());

    // Simulate a remote file creation by duplicating an existing file
    SyncName newTestFileName =
            Str("testSimultaneousChanges_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");
    DuplicateJob job(_syncPal->vfs(), _driveDbId, testExecutorFileRemoteId, newTestFileName);
    job.runSynchronously();
    NodeId remoteId = job.nodeId();

    waitForSyncToFinish(SourceLocation::currentLoc());

    // Check effect of local change on remote snapshot
    SyncTime newModTime = _syncPal->_remoteSnapshot->lastModified(testExecutorFileCopyRemoteId);
    CPPUNIT_ASSERT(newModTime > prevModTime);

    // Check effect of remote change on local snapshot
    bool found = false;
    NodeId localId;
    CPPUNIT_ASSERT(_syncPal->syncDb()->correspondingNodeId(ReplicaSide::Remote, remoteId, localId, found));
    CPPUNIT_ASSERT(found);
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->exists(localId));

    // Remove the test file
    DeleteJob deleteJob(_driveDbId, remoteId, "", "",
                        NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    deleteJob.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    std::cout << "OK" << std::endl;
}

void TestIntegration::testInconsistency() {
    LOGW_DEBUG(_logger, L"$$$$$ test inconsistency checker");
    std::cout << "test inconsistency checker : ";

    // Simulate a create by duplicating an existing file

    // Check path length
    _newTestFilePath = Str("test_inconsistency") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(220)) + Str(".txt");
    DuplicateJob job(_syncPal->vfs(), _driveDbId, testInconsistencyFileRemoteId, _newTestFilePath.native());
    job.runSynchronously();
    NodeId testSizeRemoteId = job.nodeId();

    // Check forbidden characters
    _newTestFilePath = Str("test_:inco/nsiste::ncy.txt");
    DuplicateJob job2(_syncPal->vfs(), _driveDbId, testInconsistencyFileRemoteId, _newTestFilePath.native());
    job2.runSynchronously();
    NodeId testSpecialCharsRemoteId = job2.nodeId();

    // Check name clash
    _newTestFilePath = L"test_caseSensitive.txt";
    DuplicateJob job3(_syncPal->vfs(), _driveDbId, testInconsistencyFileRemoteId, _newTestFilePath.native());
    job3.runSynchronously();
    NodeId testCaseSensitiveRemoteId1 = job3.nodeId();

    _newTestFilePath = L"test_Casesensitive.txt";
    DuplicateJob job4(_syncPal->vfs(), _driveDbId, testInconsistencyFileRemoteId, _newTestFilePath.native());
    job4.runSynchronously();
    NodeId testCaseSensitiveRemoteId2 = job4.nodeId();

    _newTestFilePath = L"TEST_casesensitive.txt";
    DuplicateJob job5(_syncPal->vfs(), _driveDbId, testInconsistencyFileRemoteId, _newTestFilePath.native());
    job5.runSynchronously();
    NodeId testCaseSensitiveRemoteId3 = job5.nodeId();

    _newTestFilePath = Str("test_long_file") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(20)) + Str(".txt");
    DuplicateJob job6(_syncPal->vfs(), _driveDbId, testLongFileRemoteId, _newTestFilePath.native());
    job6.runSynchronously();
    NodeId testLongFilePathRemoteId = job6.nodeId();

    waitForSyncToFinish(SourceLocation::currentLoc());

    bool found = false;

    // Check path length
    CPPUNIT_ASSERT(_syncPal->syncDb()->correspondingNodeId(ReplicaSide::Remote, testSizeRemoteId, _newTestFileLocalId, found));
    CPPUNIT_ASSERT(found);
    std::shared_ptr<Node> node = _syncPal->updateTree(ReplicaSide::Local)->getNodeById(_newTestFileLocalId);
    CPPUNIT_ASSERT(node);
    CPPUNIT_ASSERT(node->name().size() < 255);

    // Check forbidden characters
    CPPUNIT_ASSERT(
            _syncPal->syncDb()->correspondingNodeId(ReplicaSide::Remote, testSpecialCharsRemoteId, _newTestFileLocalId, found));
    CPPUNIT_ASSERT(found);
    std::shared_ptr<Node> node2 = _syncPal->updateTree(ReplicaSide::Local)->getNodeById(_newTestFileLocalId);
    CPPUNIT_ASSERT(node2);
    CPPUNIT_ASSERT(node2->name() == Str("test_%3ainco%3ansiste%3a%3ancy.txt"));

    // Check name clash
    CPPUNIT_ASSERT(
            _syncPal->syncDb()->correspondingNodeId(ReplicaSide::Remote, testCaseSensitiveRemoteId1, _newTestFileLocalId, found));
    CPPUNIT_ASSERT(found);
    std::shared_ptr<Node> node3 = _syncPal->updateTree(ReplicaSide::Local)->getNodeById(_newTestFileLocalId);
    CPPUNIT_ASSERT(node3);

    CPPUNIT_ASSERT(
            _syncPal->syncDb()->correspondingNodeId(ReplicaSide::Remote, testCaseSensitiveRemoteId2, _newTestFileLocalId, found));
    CPPUNIT_ASSERT(found);
    std::shared_ptr<Node> node4 = _syncPal->updateTree(ReplicaSide::Local)->getNodeById(_newTestFileLocalId);
    CPPUNIT_ASSERT(node4);
    CPPUNIT_ASSERT(node4->name() != node3->name());

    CPPUNIT_ASSERT(
            _syncPal->syncDb()->correspondingNodeId(ReplicaSide::Remote, testCaseSensitiveRemoteId3, _newTestFileLocalId, found));
    CPPUNIT_ASSERT(found);
    std::shared_ptr<Node> node5 = _syncPal->updateTree(ReplicaSide::Local)->getNodeById(_newTestFileLocalId);
    CPPUNIT_ASSERT(node5);
    CPPUNIT_ASSERT(node5->name() != node4->name());

    CPPUNIT_ASSERT(
            _syncPal->syncDb()->correspondingNodeId(ReplicaSide::Remote, testLongFilePathRemoteId, _newTestFileLocalId, found));
    CPPUNIT_ASSERT(!found);

    // Remove the test file
    DeleteJob deleteJob(_driveDbId, testSizeRemoteId, "", "",
                        NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    deleteJob.runSynchronously();
    DeleteJob deleteJob2(_driveDbId, testSpecialCharsRemoteId, "", "",
                         NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    deleteJob2.runSynchronously();
    DeleteJob deleteJob3(_driveDbId, testCaseSensitiveRemoteId1, "", "",
                         NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    deleteJob3.runSynchronously();
    DeleteJob deleteJob4(_driveDbId, testCaseSensitiveRemoteId2, "", "",
                         NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    deleteJob4.runSynchronously();
    DeleteJob deleteJob5(_driveDbId, testCaseSensitiveRemoteId3, "", "",
                         NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    deleteJob5.runSynchronously();
    DeleteJob deleteJob6(_driveDbId, testLongFilePathRemoteId, "", "",
                         NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    deleteJob6.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    std::cout << "OK" << std::endl;
}

void TestIntegration::testCreateCreatePseudoConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ test Create-Create pseudo conflict");
    std::cout << "test Create-Create pseudo conflict : " << std::endl;

    _syncPal->pause();

    SyncName newTestFileName = Str2SyncName("test_createCreatePseudoConflict_") +
                               Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str2SyncName(".txt");

    // Simulate a remote file creation by duplicating an existing file
    DuplicateJob job(_syncPal->vfs(), _driveDbId, testExecutorFileRemoteId, newTestFileName);
    job.runSynchronously();
    NodeId remoteId = job.nodeId();

    // Simulate a local file creation by duplicating an existing file
    std::filesystem::path localExecutorFolderPath = _localPath / testExecutorFolderRelativePath;
    std::filesystem::path sourceFile = localExecutorFolderPath / "test_executor.txt";

    std::filesystem::path destFile = localExecutorFolderPath / newTestFileName;

    CPPUNIT_ASSERT(std::filesystem::copy_file(sourceFile, destFile));

    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(destFile.make_preferred(), &fileStat, exists);
    NodeId prevLocalId = std::to_string(fileStat.inode);

    Utility::msleep(10000);
    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Local file should be the same
    IoHelper::getFileStat(destFile.make_preferred(), &fileStat, exists);
    NodeId newLocalId = std::to_string(fileStat.inode);
    CPPUNIT_ASSERT(newLocalId == prevLocalId);

    // Remove the test files
    DeleteJob deleteJob(_driveDbId, remoteId, "", "",
                        NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    deleteJob.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    std::cout << "OK" << std::endl;
}

void TestIntegration::testCreateCreateConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ test Create-Create conflict");
    std::cout << "test Create-Create conflict : ";

    _syncPal->pause();

    std::filesystem::path newTestFileName =
            Str("test_createCreateConflict_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");

    // Simulate a remote file creation by duplicating an existing file
    DuplicateJob job(_syncPal->vfs(), _driveDbId, testExecutorFileRemoteId, newTestFileName.native());
    job.runSynchronously();
    NodeId remoteId = job.nodeId();

    // Simulate a local file creation by duplicating an existing file
    std::filesystem::path localExecutorFolderPath = _localPath / testExecutorFolderRelativePath;
    std::filesystem::path sourceFile = localExecutorFolderPath / "test_executor_copy.txt";

    std::filesystem::path destFile = localExecutorFolderPath / newTestFileName;

    CPPUNIT_ASSERT(std::filesystem::copy_file(sourceFile, destFile));

    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(destFile.make_preferred(), &fileStat, exists);
    NodeId prevLocalId = std::to_string(fileStat.inode);

    Utility::msleep(10000);
    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Remote file should have been downloaded with previous name
    IoHelper::getFileStat(destFile.make_preferred(), &fileStat, exists);
    NodeId newLocalId = std::to_string(fileStat.inode);
    CPPUNIT_ASSERT(newLocalId != prevLocalId);

    // Look for local file that have been renamed (and excluded from sync)
    bool found = false;
    std::filesystem::path localExcludedPath;
    for (auto const &dir_entry: std::filesystem::directory_iterator{localExecutorFolderPath}) {
        if (Utility::startsWith(dir_entry.path().filename().native(), newTestFileName.stem().native() + Str("_conflict_"))) {
            found = true;
            localExcludedPath = dir_entry.path();
            break;
        }
    }
    CPPUNIT_ASSERT(found);

    // Remove the test files
    DeleteJob deleteJob(_driveDbId, remoteId, "", "",
                        NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    deleteJob.runSynchronously();

    std::filesystem::remove(localExcludedPath);

    waitForSyncToFinish(SourceLocation::currentLoc());

    std::cout << "OK" << std::endl;
}

void TestIntegration::testEditEditPseudoConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ test Edit-Edit pseudo conflict");
    std::cout << "test Edit-Edit pseudo conflict : ";

    _syncPal->pause();

    // Edit test file locally
    std::filesystem::path localExecutorFolderPath = _localPath / testExecutorFolderRelativePath;
    std::filesystem::path sourceFile = localExecutorFolderPath / "test_executor_copy.txt";

    SyncName testCallStr = Str(R"(echo "This is an edit test )") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) +
                           Str(R"(" >> ")") + sourceFile.make_preferred().native() + Str(R"(")");
    std::system(SyncName2Str(testCallStr).c_str());

    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(sourceFile.make_preferred(), &fileStat, exists);
    NodeId prevLocalId = std::to_string(fileStat.inode);

    // Upload this file manually so it simulate a remote edit
    UploadJob job(_syncPal->vfs(), _driveDbId, sourceFile, sourceFile.filename().native(), testExecutorFolderRemoteId,
                  fileStat.creationTime, fileStat.modificationTime);
    job.runSynchronously();

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Local file should be the same
    IoHelper::getFileStat(sourceFile.make_preferred().native().c_str(), &fileStat, exists);
    NodeId newLocalId = std::to_string(fileStat.inode);
    CPPUNIT_ASSERT(newLocalId == prevLocalId);

    std::cout << "OK" << std::endl;
}

void TestIntegration::testEditEditConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ test Edit-Edit conflict");
    std::cout << "test Edit-Edit conflict : ";

    _syncPal->pause();

    // Edit test file locally
    std::filesystem::path localExecutorFolderPath = _localPath / testExecutorFolderRelativePath;
    std::filesystem::path sourceFile = localExecutorFolderPath / "test_executor_copy.txt";

    SyncName testCallStr = Str(R"(echo "This is an edit test )") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) +
                           Str(R"(" >> ")") + sourceFile.make_preferred().native() + Str(R"(")");
    std::system(SyncName2Str(testCallStr).c_str());

    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(sourceFile.make_preferred().native().c_str(), &fileStat, exists);
    NodeId prevLocalId = std::to_string(fileStat.inode);

    // Upload this file manually so it simulate a remote edit
    UploadJob job(_syncPal->vfs(), _driveDbId, sourceFile, sourceFile.filename().native(), testExecutorFolderRemoteId,
                  fileStat.creationTime, fileStat.modificationTime);
    job.runSynchronously();

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    // Edit again the local file
    testCallStr = Str(R"(echo "This is an edit test )") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) +
                  Str(R"(" >> ")") + sourceFile.make_preferred().native() + Str(R"(")");
    std::system(SyncName2Str(testCallStr).c_str());

    Utility::msleep(1000);

    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Remote file should have been downloaded with previous name
    IoHelper::getFileStat(sourceFile.make_preferred(), &fileStat, exists);
    NodeId newLocalId = std::to_string(fileStat.inode);
    CPPUNIT_ASSERT(newLocalId != prevLocalId);

    // Look for local file that have been renamed (and excluded from sync)
    bool found = false;
    std::filesystem::path localExcludedPath;
    for (auto const &dir_entry: std::filesystem::directory_iterator{localExecutorFolderPath}) {
        if (Utility::startsWith(dir_entry.path().filename().native(), sourceFile.stem().native() + Str("_conflict_"))) {
            found = true;
            localExcludedPath = dir_entry.path();
            break;
        }
    }
    CPPUNIT_ASSERT(found);

    // Remove the test files
    std::filesystem::remove(localExcludedPath);

    std::cout << "OK" << std::endl;
}

void TestIntegration::testMoveCreateConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ test Move-Create conflict");
    std::cout << "test Move-Create conflict : ";

    LOGW_DEBUG(_logger, L"----- test Move-Create conflict : Init phase");
    // Init: simulate a remote file creation by duplicating an existing file
    std::filesystem::path newTestFileName =
            Str("test_moveCreateConflict_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");

    DuplicateJob initJob(_syncPal->vfs(), _driveDbId, testExecutorFileRemoteId, newTestFileName.native());
    initJob.runSynchronously();
    NodeId initRemoteId = initJob.nodeId();

    waitForSyncToFinish(SourceLocation::currentLoc());
    _syncPal->pause();

    LOGW_DEBUG(_logger, L"----- test Move-Create conflict : Setup phase");
    std::filesystem::path localExecutorFolderPath = _localPath / testExecutorFolderRelativePath;
    std::filesystem::path sourceFile = localExecutorFolderPath / newTestFileName;

    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(sourceFile.make_preferred(), &fileStat, exists);

    // Simulate a remote create by uploading the file in "test_executor_sub" folder
    UploadJob createJob(_syncPal->vfs(), _driveDbId, sourceFile, sourceFile.filename().native(), testExecutorSubFolderRemoteId,
                        fileStat.creationTime, fileStat.modificationTime);
    createJob.runSynchronously();
    NodeId remoteId = createJob.nodeId();

    // Move created file in folder "test_executor_sub" on local replica
    std::filesystem::path destFile = localExecutorFolderPath / testExecutorSubFolderName / newTestFileName;
    std::filesystem::rename(sourceFile, destFile);

    IoHelper::getFileStat(destFile.make_preferred(), &fileStat, exists);
    NodeId prevLocalId = std::to_string(fileStat.inode);

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    LOGW_DEBUG(_logger, L"----- test Move-Create conflict : Resolution phase");

    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    LOGW_DEBUG(_logger, L"----- test Move-Create conflict : Test phase");

    // Remote file should have been downloaded with previous name
    IoHelper::getFileStat(destFile.make_preferred(), &fileStat, exists);
    NodeId newLocalId = std::to_string(fileStat.inode);
    CPPUNIT_ASSERT(newLocalId != prevLocalId);

    // Look for local file that have been renamed (and excluded from sync)
    bool found = false;
    std::filesystem::path localExcludedPath;
    for (auto const &dir_entry: std::filesystem::directory_iterator{localExecutorFolderPath / testExecutorSubFolderName}) {
        if (Utility::startsWith(dir_entry.path().filename().string(), sourceFile.stem().string() + "_conflict_")) {
            found = true;
            localExcludedPath = dir_entry.path();
            break;
        }
    }
    CPPUNIT_ASSERT(found);

    // Check on remote that both init test file and duplicated test file exists
    GetFileInfoJob fileInfoJob1(_driveDbId, initRemoteId);
    fileInfoJob1.runSynchronously();
    CPPUNIT_ASSERT(!fileInfoJob1.hasHttpError());
    GetFileInfoJob fileInfoJob2(_driveDbId, remoteId);
    fileInfoJob2.runSynchronously();
    CPPUNIT_ASSERT(!fileInfoJob2.hasHttpError());

    LOGW_DEBUG(_logger, L"----- test Move-Create conflict : Clean phase");

    // Remove the test files
    _syncPal->pause();

    Utility::msleep(5000);

    DeleteJob deleteJob1(_driveDbId, initRemoteId, "", "",
                         NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    deleteJob1.runSynchronously();
    DeleteJob deleteJob2(_driveDbId, remoteId, "", "",
                         NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    deleteJob2.runSynchronously();

    std::filesystem::remove(localExcludedPath);

    Utility::msleep(5000);

    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    std::cout << "OK" << std::endl;
}

void TestIntegration::testEditDeleteConflict1() {
    LOGW_DEBUG(_logger, L"$$$$$ test Edit-Delete conflict 1");
    std::cout << "test Edit-Delete conflict 1 : ";

    // Init: simulate a remote file creation by duplicating an existing file
    std::filesystem::path newTestFileName =
            Str("test_editDeleteConflict1_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");

    DuplicateJob initJob(_syncPal->vfs(), _driveDbId, testExecutorFileRemoteId, newTestFileName.native());
    initJob.runSynchronously();
    NodeId initRemoteId = initJob.nodeId();
    waitForSyncToFinish(SourceLocation::currentLoc());

    _syncPal->pause();

    // Delete file on remote replica
    DeleteJob deleteJob(_driveDbId, initRemoteId, "", "",
                        NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    deleteJob.runSynchronously();

    // Edit file on local replica
    std::filesystem::path localExecutorFolderPath = _localPath / testExecutorFolderRelativePath;
    std::filesystem::path sourceFile = localExecutorFolderPath / newTestFileName;

    SyncName testCallStr = Str(R"(echo "This is an edit test )") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) +
                           Str(R"(" >> ")") + sourceFile.make_preferred().native() + Str(R"(")");
    std::system(SyncName2Str(testCallStr).c_str());

    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(sourceFile.make_preferred(), &fileStat, exists);
    NodeId localId = std::to_string(fileStat.inode);

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Edit operation must win, so local file should not have changed
    CPPUNIT_ASSERT(_syncPal->_localSnapshot->exists(localId));

    // Remote file should have been re-uploaded
    // File with same name should exist
    GetFileListJob job(_driveDbId, testExecutorFolderRemoteId);
    job.runSynchronously();

    Poco::JSON::Object::Ptr resObj = job.jsonRes();
    CPPUNIT_ASSERT(resObj);

    bool newFileFound = false;
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    CPPUNIT_ASSERT(dataArray);

    NodeId remoteId;
    for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
        Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
        if (newTestFileName.filename().string() == obj->get(nameKey).toString()) {
            newFileFound = true;
            remoteId = obj->get(idKey).toString();
            break;
        }
    }
    CPPUNIT_ASSERT(newFileFound);
    // But initial remote ID should have changed
    GetFileInfoJob fileInfoJob(_driveDbId, initRemoteId);
    fileInfoJob.runSynchronously();
    CPPUNIT_ASSERT(fileInfoJob.hasHttpError());

    // Remove the test files
    DeleteJob deleteJob1(_driveDbId, remoteId, "", "",
                         NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    deleteJob1.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    std::cout << "OK" << std::endl;
}

void TestIntegration::testEditDeleteConflict2() {
    LOGW_DEBUG(_logger, L"$$$$$ test Edit-Delete conflict 2");
    std::cout << "test Edit-Delete conflict 2 : ";

    // Init: create a folder and duplicate an existing file inside it
    SyncName dirName = Str("test_dir_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10));
    CreateDirJob initCreateDirJob(nullptr, _driveDbId, dirName, testExecutorFolderRemoteId, dirName);
    initCreateDirJob.runSynchronously();

    // Extract dir ID
    NodeId dirRemoteId = initCreateDirJob.nodeId();
    CPPUNIT_ASSERT(!dirRemoteId.empty());

    // Simulate a create into this directory
    std::filesystem::path newTestFileName =
            Str("test_editDeleteConflict2_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");

    CopyToDirectoryJob initJob(_driveDbId, testExecutorFileRemoteId, dirRemoteId, newTestFileName.native());
    initJob.runSynchronously();
    NodeId initFileRemoteId = initJob.nodeId();
    waitForSyncToFinish(SourceLocation::currentLoc());

    _syncPal->pause();

    // Delete dir on remote replica
    DeleteJob deleteJob(_driveDbId, dirRemoteId, "", "",
                        NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    deleteJob.runSynchronously();

    // Edit file on local replica
    std::filesystem::path localExecutorFolderPath = _localPath / testExecutorFolderRelativePath / dirName;
    std::filesystem::path sourceFile = localExecutorFolderPath / newTestFileName;

    SyncName testCallStr = Str(R"(echo "This is an edit test )") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) +
                           Str(R"(" >> ")") + sourceFile.make_preferred().native() + Str(R"(")");
    std::system(SyncName2Str(testCallStr).c_str());

    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(sourceFile.make_preferred(), &fileStat, exists);
    NodeId localFileId = std::to_string(fileStat.inode);

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Look for local file in root directory that have been renamed (and excluded from sync)
    bool found = false;
    std::filesystem::path localExcludedPath;
    for (auto const &dir_entry: std::filesystem::directory_iterator{_localPath}) {
        if (Utility::startsWith(dir_entry.path().filename().string(), newTestFileName.stem().string() + "_conflict_")) {
            found = true;
            localExcludedPath = dir_entry.path();
            break;
        }
    }
    CPPUNIT_ASSERT(found);

    // Remove the test files
    std::filesystem::remove(localExcludedPath);

    std::cout << "OK" << std::endl;
}

const std::string testConflictFolderName = "test_conflict";
const std::string testConflictFolderId = "451588";
const std::string aRefFolderId = "451591";

void testMoveDeleteConflict_initPhase(int driveDbId, NodeId &aRemoteId, NodeId &rRemoteId, NodeId &sRemoteId, NodeId &qRemoteId) {
    // Duplicate the test folder
    DuplicateJob initDuplicateJob(nullptr, driveDbId, aRefFolderId, Str("A"));
    initDuplicateJob.runSynchronously();
    aRemoteId = initDuplicateJob.nodeId();
    CPPUNIT_ASSERT(!aRemoteId.empty());

    // Find the other folder IDs
    {
        GetFileListJob initJob(driveDbId, aRemoteId);
        initJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = initJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "R") {
                rRemoteId = obj->get(idKey).toString();
            } else if (obj->get(nameKey).toString() == "S") {
                sRemoteId = obj->get(idKey).toString();
            }
        }
    }
    CPPUNIT_ASSERT(!rRemoteId.empty());
    CPPUNIT_ASSERT(!sRemoteId.empty());

    {
        GetFileListJob initJob(driveDbId, rRemoteId);
        initJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = initJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "Q") {
                qRemoteId = obj->get(idKey).toString();
            }
        }
    }
    CPPUNIT_ASSERT(!qRemoteId.empty());
}

// See p.85, case 1
void TestIntegration::testMoveDeleteConflict1() {
    LOGW_DEBUG(_logger, L"$$$$$ test Move-Delete conflict 1");
    std::cout << "test Move-Delete conflict 1: ";

    // Init phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 1 : Init phase");

    NodeId aRemoteId;
    NodeId rRemoteId;
    NodeId sRemoteId;
    NodeId qRemoteId;
    testMoveDeleteConflict_initPhase(_driveDbId, aRemoteId, rRemoteId, sRemoteId, qRemoteId);

    waitForSyncToFinish(SourceLocation::currentLoc());
    _syncPal->pause();

    // Setup phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 1 : Setup phase");

    // On local replica
    // Delete S
    {
        const SyncPalInfo syncPalInfo(_driveDbId, _localPath);
        LocalDeleteJob localDeleteJob(syncPalInfo, testExecutorFolderRelativePath / testConflictFolderName / "A/S", false,
                                      sRemoteId);
        localDeleteJob.setBypassCheck(true);
        localDeleteJob.runSynchronously();
    }

    // Rename A into B
    {
        LocalMoveJob localMoveJob(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "A",
                                  _localPath / testExecutorFolderRelativePath / testConflictFolderName / "B");
        localMoveJob.runSynchronously();
    }

    // On remote replica
    // Delete A
    {
        DeleteJob deleteJob(_driveDbId, aRemoteId, "", "",
                            NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
        deleteJob.runSynchronously();
    }

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 1 : Resolution phase");
    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Test phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 1 : Test phase");

    // On remote
    bool sFound = false;
    {
        bool found = false;
        GetFileListJob testJob(_driveDbId, testConflictFolderId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "B") {
                aRemoteId = obj->get(idKey).toString();
                found = true;
            } else if (obj->get(nameKey).toString() == "S") {
                sFound = true;
            }
        }
        CPPUNIT_ASSERT(found);
    }

    {
        bool found = false;
        GetFileListJob testJob(_driveDbId, aRemoteId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "R") {
                rRemoteId = obj->get(idKey).toString();
                found = true;
            } else if (obj->get(nameKey).toString() == "S") {
                sFound = true;
            }
        }
        CPPUNIT_ASSERT(found);
    }

    {
        bool found = false;
        GetFileListJob testJob(_driveDbId, rRemoteId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "Q") {
                rRemoteId = obj->get(idKey).toString();
                found = true;
            } else if (obj->get(nameKey).toString() == "S") {
                sFound = true;
            }
        }
        CPPUNIT_ASSERT(found);
    }

    CPPUNIT_ASSERT(!sFound);

    // On local
    // Look for local file that have been renamed (and excluded from sync)
    CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "B/R/Q"));
    CPPUNIT_ASSERT(!std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "B/S"));

    // Clean phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 1 : Clean phase");

    // Remove the test files
    DeleteJob finalDeleteJob(_driveDbId, aRemoteId, "", "",
                             NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    finalDeleteJob.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    std::cout << "OK" << std::endl;
}

// See p.85, case 2
void TestIntegration::testMoveDeleteConflict2() {
    LOGW_DEBUG(_logger, L"$$$$$ test Move-Delete conflict 2");
    std::cout << "test Move-Delete conflict 2: ";

    // Init phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 2 : Init phase");

    NodeId aRemoteId;
    NodeId rRemoteId;
    NodeId sRemoteId;
    NodeId qRemoteId;
    testMoveDeleteConflict_initPhase(_driveDbId, aRemoteId, rRemoteId, sRemoteId, qRemoteId);

    waitForSyncToFinish(SourceLocation::currentLoc());
    _syncPal->pause();

    // Setup phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 2 : Setup phase");

    // On local replica
    // Delete A
    LocalDeleteJob localDeleteJob(SyncPalInfo{_driveDbId, _localPath},
                                  testExecutorFolderRelativePath / testConflictFolderName / "A", false, aRemoteId);
    localDeleteJob.setBypassCheck(true);
    localDeleteJob.runSynchronously();

    // On remote replica
    // Rename A
    RenameJob setupRenameJob1(_syncPal->vfs(), _driveDbId, aRemoteId, Str("B"));
    setupRenameJob1.runSynchronously();

    // Edit Q
    std::filesystem::path tmpFile = std::filesystem::temp_directory_path() / L"tmp_test_file.txt";
    SyncName testCallStr = Str(R"(echo "This is an edit test )") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) +
                           Str(R"(" >> ")") + tmpFile.make_preferred().native() + Str(R"(")");
    std::system(SyncName2Str(testCallStr).c_str());

    UploadJob setupUploadJob(_syncPal->vfs(), _driveDbId, tmpFile, Str("Q"), rRemoteId, 0, 0);
    setupUploadJob.runSynchronously();

    // Create A/S/X
    NodeId xRemoteId;
    CreateDirJob setupCreateDirJob(_syncPal->vfs(), _driveDbId, Str("X"), sRemoteId, Str("X"));
    setupCreateDirJob.runSynchronously();
    xRemoteId = setupCreateDirJob.nodeId();

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 2 : Resolution phase");
    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Test phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 2 : Test phase");

    // On remote
    {
        bool found = false;
        GetFileListJob testJob(_driveDbId, testConflictFolderId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "B") {
                aRemoteId = obj->get(idKey).toString();
                found = true;
            }
        }
        CPPUNIT_ASSERT(found);
    }

    {
        bool rFound = false;
        bool sFound = false;
        GetFileListJob testJob(_driveDbId, aRemoteId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "R") {
                rRemoteId = obj->get(idKey).toString();
                rFound = true;
            } else if (obj->get(nameKey).toString() == "S") {
                sRemoteId = obj->get(idKey).toString();
                sFound = true;
            }
        }
        CPPUNIT_ASSERT(rFound);
        CPPUNIT_ASSERT(sFound);
    }

    {
        bool qFound = false;
        GetFileListJob testJob(_driveDbId, rRemoteId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "Q") {
                qRemoteId = obj->get(idKey).toString();
                qFound = true;
            }
        }
        CPPUNIT_ASSERT(qFound);
    }

    {
        bool xFound = false;
        GetFileListJob testJob(_driveDbId, sRemoteId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "X") {
                xRemoteId = obj->get(idKey).toString();
                xFound = true;
            }
        }
        CPPUNIT_ASSERT(xFound);
    }

    // On local
    // Look for local file that have been renamed (and excluded from sync)
    CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "B/R/Q"));
    CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "B/S/X"));

    // Clean phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 2 : Clean phase");

    // Remove the test files
    DeleteJob finalDeleteJob(_driveDbId, aRemoteId, "", "",
                             NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    finalDeleteJob.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    std::cout << "OK" << std::endl;
}

// See p.86, case 3
void TestIntegration::testMoveDeleteConflict3() {
    LOGW_DEBUG(_logger, L"$$$$$ test Move-Delete conflict 3");
    std::cout << "test Move-Delete conflict 3: ";

    // Init phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 3 : Init phase");

    NodeId aRemoteId;
    NodeId rRemoteId;
    NodeId sRemoteId;
    NodeId qRemoteId;
    testMoveDeleteConflict_initPhase(_driveDbId, aRemoteId, rRemoteId, sRemoteId, qRemoteId);

    waitForSyncToFinish(SourceLocation::currentLoc());
    _syncPal->pause();

    // Setup phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 3 : Setup phase");

    // On local replica
    // Rename A into B
    LocalMoveJob setupMoveJob1(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "A",
                               _localPath / testExecutorFolderRelativePath / testConflictFolderName / "B");
    setupMoveJob1.runSynchronously();

    // Move S under root
    LocalMoveJob setupMoveJob2(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "B/S",
                               _localPath / testExecutorFolderRelativePath / testConflictFolderName / "S");
    setupMoveJob2.runSynchronously();

    // On remote replica
    // Delete A
    DeleteJob setupDeleteJob(_driveDbId, aRemoteId, "", "",
                             NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    setupDeleteJob.runSynchronously();

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 3 : Resolution phase");
    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Test phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 3 : Test phase");

    // On remote
    {
        bool foundB = false;
        bool foundS = false;
        GetFileListJob testJob(_driveDbId, testConflictFolderId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "B") {
                aRemoteId = obj->get(idKey).toString();
                foundB = true;
            } else if (obj->get(nameKey).toString() == "S") {
                sRemoteId = obj->get(idKey).toString();
                foundS = true;
            }
        }
        CPPUNIT_ASSERT(foundB);
        CPPUNIT_ASSERT(foundS);
    }

    {
        bool found = false;
        GetFileListJob testJob(_driveDbId, aRemoteId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "R") {
                rRemoteId = obj->get(idKey).toString();
                found = true;
            }
        }
        CPPUNIT_ASSERT(found);
    }

    {
        bool found = false;
        GetFileListJob testJob(_driveDbId, rRemoteId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "Q") {
                qRemoteId = obj->get(idKey).toString();
                found = true;
            }
        }
        CPPUNIT_ASSERT(found);
    }

    // On local
    // Look for local file that have been renamed (and excluded from sync)
    CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "B/R/Q"));
    CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "S"));

    // Clean phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 3 : Clean phase");

    // Remove the test files
    DeleteJob finalDeleteJob1(_driveDbId, aRemoteId, "", "",
                              NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    finalDeleteJob1.runSynchronously();

    DeleteJob finalDeleteJob2(_driveDbId, sRemoteId, "", "",
                              NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    finalDeleteJob2.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());
    std::cout << "OK" << std::endl;
}

// See p.86, case 4
void TestIntegration::testMoveDeleteConflict4() {
    LOGW_DEBUG(_logger, L"$$$$$ test Move-Delete conflict 4");
    std::cout << "test Move-Delete conflict 4: ";

    // Init phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 4 : Init phase");

    NodeId aRemoteId;
    NodeId rRemoteId;
    NodeId sRemoteId;
    NodeId qRemoteId;
    testMoveDeleteConflict_initPhase(_driveDbId, aRemoteId, rRemoteId, sRemoteId, qRemoteId);

    waitForSyncToFinish(SourceLocation::currentLoc());
    _syncPal->pause();

    // Setup phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 4 : Setup phase");

    // On local replica
    // Move S under root
    LocalMoveJob setupMoveJob(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "A/S",
                              _localPath / testExecutorFolderRelativePath / testConflictFolderName / "S");
    setupMoveJob.runSynchronously();

    LocalDeleteJob setupDeleteJob(SyncPalInfo{_driveDbId, _localPath},
                                  testExecutorFolderRelativePath / testConflictFolderName / "A", false, aRemoteId);
    setupDeleteJob.setBypassCheck(true);
    setupDeleteJob.runSynchronously();

    // On remote replica
    // Rename A into B
    RenameJob setupRenameJob(_syncPal->vfs(), _driveDbId, aRemoteId, Str("B"));
    setupRenameJob.runSynchronously();

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 4 : Resolution phase");
    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Test phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 4 : Test phase");

    // On remote
    {
        bool foundB = false;
        bool foundS = false;
        GetFileListJob testJob(_driveDbId, testConflictFolderId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "B") {
                aRemoteId = obj->get(idKey).toString();
                foundB = true;
            } else if (obj->get(nameKey).toString() == "S") {
                sRemoteId = obj->get(idKey).toString();
                foundS = true;
            }
        }
        CPPUNIT_ASSERT(foundB);
        CPPUNIT_ASSERT(foundS);
    }

    {
        bool found = false;
        GetFileListJob testJob(_driveDbId, aRemoteId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "R") {
                rRemoteId = obj->get(idKey).toString();
                found = true;
            }
        }
        CPPUNIT_ASSERT(found);
    }

    {
        bool found = false;
        GetFileListJob testJob(_driveDbId, rRemoteId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "Q") {
                qRemoteId = obj->get(idKey).toString();
                found = true;
            }
        }
        CPPUNIT_ASSERT(found);
    }

    // On local
    // Look for local file that have been renamed (and excluded from sync)
    CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "B/R/Q"));
    CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "S"));

    // Clean phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 4 : Clean phase");

    // Remove the test files
    DeleteJob finalDeleteJob1(_driveDbId, aRemoteId, "", "",
                              NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    finalDeleteJob1.runSynchronously();

    DeleteJob finalDeleteJob2(_driveDbId, sRemoteId, "", "",
                              NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    finalDeleteJob2.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    std::cout << "OK" << std::endl;
}

// See p.87, case 5
void TestIntegration::testMoveDeleteConflict5() {
    // Note: This case is in fact solve as a Move-ParentDelete conflict

    LOGW_DEBUG(_logger, L"$$$$$ test Move-Delete conflict 5");
    std::cout << "test Move-Delete conflict 5: ";

    // Init phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 5 : Init phase");

    NodeId aRemoteId;
    NodeId rRemoteId;
    NodeId sRemoteId;
    NodeId qRemoteId;
    testMoveDeleteConflict_initPhase(_driveDbId, aRemoteId, rRemoteId, sRemoteId, qRemoteId);

    waitForSyncToFinish(SourceLocation::currentLoc());
    _syncPal->pause();

    // Setup phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 5 : Setup phase");

    // On local replica
    // Move S under root
    LocalMoveJob setupMoveJob(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "A/R",
                              _localPath / testExecutorFolderRelativePath / testConflictFolderName / "A/X");
    setupMoveJob.runSynchronously();

    // On remote replica
    // Rename A into B
    DeleteJob setupDeleteJob(_driveDbId, aRemoteId, "", "",
                             NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    setupDeleteJob.runSynchronously();

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 5 : Resolution phase");
    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Test phase
    LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 5 : Test phase");

    // On remote
    {
        bool foundA = false;
        GetFileListJob testJob(_driveDbId, testConflictFolderId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "A") {
                aRemoteId = obj->get(idKey).toString();
                foundA = true;
            }
        }
        CPPUNIT_ASSERT(!foundA);
    }

    // On local
    // Look for local file that have been renamed (and excluded from sync)
    CPPUNIT_ASSERT(!std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "A"));

    std::cout << "OK" << std::endl;
}

void TestIntegration::testMoveParentDeleteConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ test MoveParent-Delete conflict");
    std::cout << "test MoveParent-Delete conflict : ";

    // Init phase
    LOGW_DEBUG(_logger, L"----- test MoveParent-Delete conflict : Init phase");

    NodeId aRemoteId;
    NodeId rRemoteId;
    NodeId sRemoteId;
    NodeId qRemoteId;
    testMoveDeleteConflict_initPhase(_driveDbId, aRemoteId, rRemoteId, sRemoteId, qRemoteId);

    waitForSyncToFinish(SourceLocation::currentLoc());
    _syncPal->pause();

    // Setup phase
    LOGW_DEBUG(_logger, L"----- test MoveParent-Delete conflict : Setup phase");

    // On local replica
    // Delete R
    LocalDeleteJob setupDeleteJob(SyncPalInfo{_driveDbId, _localPath},
                                  testExecutorFolderRelativePath / testConflictFolderName / "A/R", false, aRemoteId);
    setupDeleteJob.setBypassCheck(true);
    setupDeleteJob.runSynchronously();

    // On remote replica
    // Move S into R
    MoveJob setupMoveJob(_syncPal->vfs(), _driveDbId, "", sRemoteId, rRemoteId);
    setupMoveJob.runSynchronously();

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    LOGW_DEBUG(_logger, L"----- test MoveParent-Delete conflict : Resolution phase");
    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Test phase
    LOGW_DEBUG(_logger, L"----- test MoveParent-Delete conflict : Test phase");

    // On remote
    {
        bool foundR = false;
        bool foundS = false;
        GetFileListJob testJob(_driveDbId, aRemoteId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "R") {
                rRemoteId = obj->get(idKey).toString();
                foundR = true;
            } else if (obj->get(nameKey).toString() == "S") {
                sRemoteId = obj->get(idKey).toString();
                foundS = true;
            }
        }
        CPPUNIT_ASSERT(!foundR);
        CPPUNIT_ASSERT(foundS);
    }

    // On local
    // Look for local file that have been renamed (and excluded from sync)
    CPPUNIT_ASSERT(!std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "A/R/Q"));
    CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "A/S"));

    // Clean phase
    LOGW_DEBUG(_logger, L"----- test MoveParent-Delete conflict : Clean phase");

    // Remove the test files
    DeleteJob finalDeleteJob(_driveDbId, aRemoteId, "", "",
                             NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    finalDeleteJob.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    std::cout << "OK" << std::endl;
}

void TestIntegration::testCreateParentDeleteConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ test Create-ParentDelete conflict");
    std::cout << "test Create-ParentDelete conflict : ";

    // Init phase
    LOGW_DEBUG(_logger, L"----- test Create-ParentDelete conflict : Init phase");

    NodeId aRemoteId;
    NodeId rRemoteId;
    NodeId sRemoteId;
    NodeId qRemoteId;
    testMoveDeleteConflict_initPhase(_driveDbId, aRemoteId, rRemoteId, sRemoteId, qRemoteId);

    waitForSyncToFinish(SourceLocation::currentLoc());
    _syncPal->pause();

    // Setup phase
    LOGW_DEBUG(_logger, L"----- test Create-ParentDelete conflict : Setup phase");

    // On local replica
    // Create file in R
    SyncName tmpFileName =
            Str("testCreateParentDelete_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");
    SyncPath tmpFile = _localPath / testExecutorFolderRelativePath / testConflictFolderName / Str("A/R") / tmpFileName;
    SyncName testCallStr = Str(R"(echo "This is an edit test )") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) +
                           Str(R"(" >> ")") + tmpFile.make_preferred().native() + Str(R"(")");
    std::system(SyncName2Str(testCallStr).c_str());

    // On remote replica
    // Delete A
    DeleteJob setupDeleteJob(_driveDbId, aRemoteId, "", "",
                             NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    setupDeleteJob.runSynchronously();

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    LOGW_DEBUG(_logger, L"----- test Create-ParentDelete : Resolution phase");
    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Test phase
    LOGW_DEBUG(_logger, L"----- test Create-ParentDelete conflict : Test phase");

    // On remote
    NodeId testFileRemoteId;
    {
        // Files with "_conflict_" suffix are excluded from sync
        bool found = false;
        GetFileListJob testJob(_driveDbId, "1");
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (Utility::startsWith(obj->get(nameKey).toString(), tmpFile.stem().string() + "_conflict_")) {
                testFileRemoteId = obj->get(idKey).toString();
                found = true;
            }
        }
        CPPUNIT_ASSERT(!found);
    }

    {
        // Directory A should have been deleted
        bool found = false;
        GetFileListJob testJob(_driveDbId, testConflictFolderId);
        testJob.runSynchronously();
        Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
        CPPUNIT_ASSERT(resObj);
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        CPPUNIT_ASSERT(dataArray);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj->get(nameKey).toString() == "A") {
                found = true;
            }
        }
        CPPUNIT_ASSERT(!found);
    }

    // On local
    // Files with "_conflict_" suffix should be present in root directory
    bool found = false;
    std::filesystem::path localExcludedPath;
    for (auto const &dir_entry: std::filesystem::directory_iterator{_localPath}) {
        if (Utility::startsWith(dir_entry.path().filename().native(), tmpFile.stem().native() + Str("_conflict_"))) {
            found = true;
            localExcludedPath = dir_entry.path();
            break;
        }
    }
    CPPUNIT_ASSERT(found);

    CPPUNIT_ASSERT(!std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "A"));

    // Clean phase
    LOGW_DEBUG(_logger, L"----- test Create-ParentDelete conflict : Clean phase");

    // Remove the test files
    std::filesystem::remove(localExcludedPath);

    waitForSyncToFinish(SourceLocation::currentLoc());

    std::cout << "OK" << std::endl;
}

void TestIntegration::testMoveMoveSourcePseudoConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ test Move-MoveSource pseudo conflict");
    std::cout << "test Move-MoveSource pseudo conflict : ";

    // Init phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveSource pseudo conflict : Init phase");

    // Duplicate the test file
    SyncName testFileName = Str("testMoveMoveSourcePseudoConflict_") +
                            Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");
    DuplicateJob initJob(_syncPal->vfs(), _driveDbId, testExecutorFileRemoteId, testFileName);
    initJob.runSynchronously();
    NodeId testFileRemoteId = initJob.nodeId();

    waitForSyncToFinish(SourceLocation::currentLoc());
    _syncPal->pause();

    // Setup phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveSource pseudo conflict : Setup phase");

    // On local replica
    // Move test file into executor sub folder
    SyncPath sourcePath = _localPath / testExecutorFolderRelativePath / testFileName;
    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(sourcePath.make_preferred(), &fileStat, exists);
    NodeId prevLocalId = std::to_string(fileStat.inode);

    SyncPath destPath = _localPath / testExecutorFolderRelativePath / testExecutorSubFolderName / testFileName;
    LocalMoveJob setupLocalMoveJob(sourcePath, destPath);
    setupLocalMoveJob.runSynchronously();

    // On remote replica
    // Move test file into executor sub folder
    MoveJob setupRemoteMoveJob(_syncPal->vfs(), _driveDbId, "", testFileRemoteId, testExecutorSubFolderRemoteId);
    setupRemoteMoveJob.runSynchronously();

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    LOGW_DEBUG(_logger, L"----- test Move-MoveSource pseudo conflict : Resolution phase");
    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Test phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveSource pseudo conflict : Test phase");

    // Local file ID should be the same
    IoHelper::getFileStat(destPath.make_preferred(), &fileStat, exists);
    NodeId newLocalId = std::to_string(fileStat.inode);
    CPPUNIT_ASSERT(newLocalId == prevLocalId);

    // Clean phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveSource pseudo conflict : Clean phase");

    // Remove the test files
    DeleteJob finalDeleteJob(_driveDbId, testFileRemoteId, "", "",
                             NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    finalDeleteJob.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    std::cout << "OK" << std::endl;
}

void TestIntegration::testMoveMoveSourceConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ test Move-MoveSource conflict");
    std::cout << "test Move-MoveSource conflict : ";

    // Init phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveSource conflict : Init phase");

    // Duplicate the test file
    SyncName testFileName = Str("testMoveMoveSourcePseudoConflict_") +
                            Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");
    DuplicateJob initJob(_syncPal->vfs(), _driveDbId, testExecutorFileRemoteId, testFileName);
    initJob.runSynchronously();
    NodeId testFileRemoteId = initJob.nodeId();

    waitForSyncToFinish(SourceLocation::currentLoc());
    _syncPal->pause();

    // Setup phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveSource conflict : Setup phase");

    // On local replica
    // Move test file into executor sub folder
    SyncPath sourcePath = _localPath / testExecutorFolderRelativePath / testFileName;
    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(sourcePath.make_preferred(), &fileStat, exists);
    NodeId prevLocalId = std::to_string(fileStat.inode);

    SyncPath destPath = _localPath / testExecutorFolderRelativePath / testConflictFolderName / testFileName;
    LocalMoveJob setupLocalMoveJob(sourcePath, destPath);
    setupLocalMoveJob.runSynchronously();

    // On remote replica
    // Move test file into executor sub folder
    MoveJob setupRemoteMoveJob(_syncPal->vfs(), _driveDbId, "", testFileRemoteId, testExecutorSubFolderRemoteId);
    setupRemoteMoveJob.runSynchronously();

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    LOGW_DEBUG(_logger, L"----- test Move-MoveSource conflict : Resolution phase");
    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Test phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveSource conflict : Test phase");

    // Remote should win
    CPPUNIT_ASSERT(!std::filesystem::exists(destPath));

    GetFileInfoJob testJob(_driveDbId, testFileRemoteId);
    testJob.runSynchronously();
    CPPUNIT_ASSERT(testJob.parentNodeId() == testExecutorSubFolderRemoteId);

    // Clean phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveSource conflict : Clean phase");

    // Remove the test files
    DeleteJob finalDeleteJob(_driveDbId, testFileRemoteId, "", "",
                             NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    finalDeleteJob.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    std::cout << "OK" << std::endl;
}

void TestIntegration::testMoveMoveDestConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ test Move-MoveDest conflict");
    std::cout << "test Move-MoveDest conflict : ";

    // Init phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveDest conflict : Init phase");
    SyncName testFileName =
            Str("testMoveMoveDestConflict_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");

    // Duplicate the remote test file
    DuplicateJob initDuplicateJob(_syncPal->vfs(), _driveDbId, testExecutorFileRemoteId, testFileName);
    initDuplicateJob.runSynchronously();
    NodeId testFileRemoteId = initDuplicateJob.nodeId();

    // Duplicate the local test file
    SyncPath sourcePath = _localPath / testExecutorFolderRelativePath / "test_executor_copy.txt";
    SyncPath destPath = _localPath / testExecutorFolderRelativePath / "test_executor_copy_tmp.txt";
    LocalCopyJob initLocalCopyJob(sourcePath, destPath);
    initLocalCopyJob.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());
    _syncPal->pause();

    // Setup phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveDest conflict : Setup phase");

    // On remote replica
    // Move the remote test file under folder "test_executor_sub"
    MoveJob setupMoveJob(_syncPal->vfs(), _driveDbId, "", testFileRemoteId, testExecutorSubFolderRemoteId);
    setupMoveJob.runSynchronously();

    // On local replica
    // Move the local test file under folder "test_executor_sub"
    sourcePath = destPath;
    destPath = _localPath / testExecutorFolderRelativePath / testExecutorSubFolderName / testFileName;
    LocalMoveJob setupLocalMoveJob(sourcePath, destPath);
    setupLocalMoveJob.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    LOGW_DEBUG(_logger, L"----- test Move-MoveDest conflict : Resolution phase");
    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Test phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveDest conflict : Test phase");

    // Remote should win
    // On local
    bool found = false;
    std::filesystem::path localExcludedPath;
    for (auto const &dir_entry:
         std::filesystem::directory_iterator{_localPath / testExecutorFolderRelativePath / testExecutorSubFolderName}) {
        if (Utility::startsWith(dir_entry.path().filename().native(), destPath.stem().native() + Str("_conflict_"))) {
            found = true;
            localExcludedPath = dir_entry.path();
            break;
        }
    }
    CPPUNIT_ASSERT(found);

    // On remote
    GetFileInfoJob testJob(_driveDbId, testFileRemoteId);
    testJob.runSynchronously();
    CPPUNIT_ASSERT(testJob.parentNodeId() == testExecutorSubFolderRemoteId);

    // Clean phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveDest conflict : Clean phase");

    // Remove the test files
    DeleteJob finalDeleteJob(_driveDbId, testFileRemoteId, "", "",
                             NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    finalDeleteJob.runSynchronously();

    std::filesystem::remove(localExcludedPath);

    waitForSyncToFinish(SourceLocation::currentLoc());

    std::cout << "OK" << std::endl;
}

void TestIntegration::testMoveMoveCycleConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ test Move-MoveCycle conflict");
    std::cout << "test Move-MoveCycle conflict : ";

    // Init phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveCycle conflict : Init phase");
    SyncName testDirA = Str("testMoveMoveDestConflictA_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10));
    SyncName testDirB = Str("testMoveMoveDestConflictB_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10));

    // Create folders
    CreateDirJob initCreateDirJobA(nullptr, _driveDbId, testDirA, testExecutorFolderRemoteId, testDirA);
    initCreateDirJobA.runSynchronously();
    NodeId testDirRemoteIdA = initCreateDirJobA.nodeId();

    CreateDirJob initCreateDirJobB(nullptr, _driveDbId, testDirB, testExecutorFolderRemoteId, testDirB);
    initCreateDirJobB.runSynchronously();
    NodeId testDirRemoteIdB = initCreateDirJobB.nodeId();

    waitForSyncToFinish(SourceLocation::currentLoc());
    _syncPal->pause();

    // Setup phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveCycle conflict : Setup phase");

    // On remote replica
    // Move A into B
    MoveJob setupMoveJob(nullptr, _driveDbId, "", testDirRemoteIdA, testDirRemoteIdB);
    setupMoveJob.runSynchronously();

    // On local replica
    // Move B into A
    SyncPath sourcePath = _localPath / testExecutorFolderRelativePath / testDirB;
    SyncPath destPath = _localPath / testExecutorFolderRelativePath / testDirA / testDirB;
    LocalMoveJob setupLocalMoveJob(sourcePath, destPath);
    setupLocalMoveJob.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
                            // request is implemented)

    LOGW_DEBUG(_logger, L"----- test Move-MoveCycle conflict : Resolution phase");
    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Test phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveCycle conflict : Test phase");

    // Remote should win
    // On remote
    GetFileInfoJob testJob(_driveDbId, testDirRemoteIdA);
    testJob.runSynchronously();
    CPPUNIT_ASSERT(testJob.parentNodeId() == testDirRemoteIdB);

    // Clean phase
    LOGW_DEBUG(_logger, L"----- test Move-MoveCycle conflict : Clean phase");

    // Remove the test files
    DeleteJob finalDeleteJob(_driveDbId, testDirRemoteIdB, "", "",
                             NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
    finalDeleteJob.runSynchronously();

    waitForSyncToFinish(SourceLocation::currentLoc());

    std::cout << "OK" << std::endl;
}

#ifdef __unix__
void TestIntegration::testNodeIdReuseFile2DirAndDir2File() {
    if (!testhelpers::isExtendedTest()) return;
    LOGW_DEBUG(_logger, L"$$$$$ testNodeIdReuseFile2DirAndDir2File");

    SyncNodeCache::instance()->update(_driveDbId, SyncNodeType::BlackList,
                                      {test_commonDocumentsNodeId}); // Exclude common documents folder
    const RemoteTemporaryDirectory remoteTempDir(_driveDbId, "1", "testNodeIdReuseFile2DirAndDir2File");
    const SyncPath relativeWorkingDirPath = remoteTempDir.name();
    const SyncPath absoluteLocalWorkingDir = _localPath / relativeWorkingDirPath;
    _syncPal->start();
    waitForSyncToFinish(SourceLocation::currentLoc());

    const auto &localSnapshot = _syncPal->liveSnapshot(ReplicaSide::Local);
    const auto &remoteSnapshot = _syncPal->liveSnapshot(ReplicaSide::Remote);
    CPPUNIT_ASSERT(!localSnapshot.itemId(relativeWorkingDirPath).empty());
    CPPUNIT_ASSERT(!remoteSnapshot.itemId(relativeWorkingDirPath).empty());

    MockIoHelperFileStat mockIoHelper;
    // Create a file with a custom inode on the local side
    mockIoHelper.setPathWithFakeInode(absoluteLocalWorkingDir / "testNodeIdReuseFile", 3);
    { const std::ofstream file((absoluteLocalWorkingDir / "testNodeIdReuseFile").string()); }
    waitForSyncToFinish(SourceLocation::currentLoc());
    CPPUNIT_ASSERT_EQUAL(NodeId("3"), localSnapshot.itemId(relativeWorkingDirPath / "testNodeIdReuseFile"));
    const NodeId remoteFileId = remoteSnapshot.itemId(relativeWorkingDirPath / "testNodeIdReuseFile");
    CPPUNIT_ASSERT(!remoteFileId.empty());
    CPPUNIT_ASSERT_EQUAL(NodeType::File, remoteSnapshot.type(remoteFileId));

    // Replace the file with a directory on the local side (with the same id)
    _syncPal->pause();
    while (!_syncPal->isPaused()) {
        Utility::msleep(100);
    }
    IoError ioError = IoError::Success;
    IoHelper::deleteItem(absoluteLocalWorkingDir / "testNodeIdReuseFile", ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    mockIoHelper.setPathWithFakeInode(absoluteLocalWorkingDir / "testNodeIdReuseDir", 3);
    IoHelper::createDirectory(absoluteLocalWorkingDir / "testNodeIdReuseDir", false, ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Check that the file has been replaced by a directory on the remote with a different ID
    const NodeId newRemoteDirId = remoteSnapshot.itemId(relativeWorkingDirPath / "testNodeIdReuseDir");
    CPPUNIT_ASSERT(!newRemoteDirId.empty());
    CPPUNIT_ASSERT(newRemoteDirId != remoteFileId);
    CPPUNIT_ASSERT_EQUAL(NodeType::Directory, remoteSnapshot.type(newRemoteDirId));
    CPPUNIT_ASSERT(!remoteSnapshot.exists(remoteFileId));

    // Replace the directory with a file on the local side with the same id
    _syncPal->pause();
    while (!_syncPal->isPaused()) {
        Utility::msleep(100);
    }
    IoHelper::deleteItem(absoluteLocalWorkingDir / "testNodeIdReuseDir", ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    { const std::ofstream file((absoluteLocalWorkingDir / "testNodeIdReuseFile").string()); }
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Check that the directory has been replaced by a file on the remote with a different ID
    const NodeId newRemoteFileId = remoteSnapshot.itemId(relativeWorkingDirPath / "testNodeIdReuseFile");
    CPPUNIT_ASSERT(newRemoteFileId != "");
    CPPUNIT_ASSERT(newRemoteFileId != newRemoteDirId);
    CPPUNIT_ASSERT(newRemoteFileId != remoteFileId);
    CPPUNIT_ASSERT(!remoteSnapshot.exists(remoteFileId));
    CPPUNIT_ASSERT(!remoteSnapshot.exists(newRemoteDirId));
    CPPUNIT_ASSERT_EQUAL(NodeType::File, remoteSnapshot.type(newRemoteFileId));
}

void TestIntegration::testNodeIdReuseFile2File() {
    if (!testhelpers::isExtendedTest()) return;
    LOGW_DEBUG(_logger, L"$$$$$ testNodeIdReuseFile2File");

    SyncNodeCache::instance()->update(_driveDbId, SyncNodeType::BlackList,
                                      {test_commonDocumentsNodeId}); // Exclude common documents folder
    const RemoteTemporaryDirectory remoteTempDir(_driveDbId, "1", "testNodeIdReuseFile2File");
    const SyncPath relativeWorkingDirPath = remoteTempDir.name();
    const SyncPath absoluteLocalWorkingDir = _localPath / relativeWorkingDirPath;
    _syncPal->start();
    waitForSyncToFinish(SourceLocation::currentLoc());

    const auto &localSnapshot = _syncPal->liveSnapshot(ReplicaSide::Local);
    const auto &remoteSnapshot = _syncPal->liveSnapshot(ReplicaSide::Remote);
    CPPUNIT_ASSERT(!localSnapshot.itemId(relativeWorkingDirPath).empty());
    CPPUNIT_ASSERT(!remoteSnapshot.itemId(relativeWorkingDirPath).empty());

    MockIoHelperFileStat mockIoHelper;
    mockIoHelper.setPathWithFakeInode(absoluteLocalWorkingDir / "testNodeIdReuseFile", 2);
    { const std::ofstream file((absoluteLocalWorkingDir / "testNodeIdReuseFile").string()); }
    waitForSyncToFinish(SourceLocation::currentLoc());
    CPPUNIT_ASSERT_EQUAL(NodeId("2"), localSnapshot.itemId(relativeWorkingDirPath / "testNodeIdReuseFile"));
    const NodeId remoteFileId = remoteSnapshot.itemId(relativeWorkingDirPath / "testNodeIdReuseFile");
    CPPUNIT_ASSERT(!remoteFileId.empty());
    CPPUNIT_ASSERT_EQUAL(NodeType::File, remoteSnapshot.type(remoteFileId));

    // Expected behavior:
    // - changed: ctime, mtime/size/content, path (chmod or + edit + move)
    //   unchanged: inode
    //   - new file on remote side
    //
    // - changed: mtime/size/content, path (edit + move)
    //   unchanged: inode, ctime
    //   - Edit + Move on remote side


    // Delete a file and create an other file with the same inode at a different path with different content and ctime
    // Expected behavior: new file on remote side
    _syncPal->pause();
    while (!_syncPal->isPaused()) {
        Utility::msleep(100);
    }

    IoError ioError = IoError::Success;
    IoHelper::deleteItem(absoluteLocalWorkingDir / "testNodeIdReuseFile", ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    mockIoHelper.setPathWithFakeInode(absoluteLocalWorkingDir / "testNodeIdReuseFile2", 2);
    { std::ofstream((absoluteLocalWorkingDir / "testNodeIdReuseFile2").string()) << "New content"; }
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());
    _syncPal->pause();
    const NodeId newRemoteFileId = remoteSnapshot.itemId(relativeWorkingDirPath / "testNodeIdReuseFile2");
    CPPUNIT_ASSERT(!newRemoteFileId.empty());
    CPPUNIT_ASSERT(remoteFileId != newRemoteFileId);
    CPPUNIT_ASSERT(!remoteSnapshot.exists(remoteFileId));

    // Edit a file and move it to a different path.
    // Expected behavior: Edit + Move on remote side
    {
        std::ofstream file(absoluteLocalWorkingDir / "testNodeIdReuseFile2");
        file << "New content2"; // Content change -> edit
        file.close();
    }
    mockIoHelper.setPathWithFakeInode(absoluteLocalWorkingDir / "testNodeIdReuseFile3", 2);
    IoHelper::moveItem(absoluteLocalWorkingDir / "testNodeIdReuseFile2", absoluteLocalWorkingDir / "testNodeIdReuseFile3",
                       ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());
    CPPUNIT_ASSERT(remoteSnapshot.itemId(relativeWorkingDirPath / "testNodeIdReuseFile2").empty());
    const NodeId newRemoteFileId2 = remoteSnapshot.itemId(relativeWorkingDirPath / "testNodeIdReuseFile3");
    CPPUNIT_ASSERT(!newRemoteFileId2.empty());
    CPPUNIT_ASSERT_EQUAL(newRemoteFileId, newRemoteFileId2);
    CPPUNIT_ASSERT_EQUAL(remoteSnapshot.size(newRemoteFileId2), localSnapshot.size("2"));
}
#endif
void TestIntegration::waitForSyncToFinish(const SourceLocation &srcLoc) {
    using namespace std::chrono;
    const auto waitForSyncToFinishStart = steady_clock::now();
    const auto timeOutDuration = minutes(2);

    // Wait for end of sync (A sync is considered ended when it stay in Idle for more than 3s
    bool ended = false;
    while (!ended) {
        CPPUNIT_ASSERT_MESSAGE(srcLoc.toString(),
                               duration_cast<minutes>(steady_clock::now() - waitForSyncToFinishStart) < timeOutDuration);

        if (_syncPal->isIdle() && !_syncPal->_localFSObserverWorker->updating() &&
            !_syncPal->_remoteFSObserverWorker->updating()) {
            const auto idleStart = steady_clock::now();
            while (_syncPal->isIdle() && duration_cast<seconds>(steady_clock::now() - idleStart) < seconds(3)) {
                CPPUNIT_ASSERT_MESSAGE(srcLoc.toString(),
                                       duration_cast<minutes>(steady_clock::now() - waitForSyncToFinishStart) < timeOutDuration);
                Utility::msleep(5);
            }
            ended = duration_cast<seconds>(steady_clock::now() - idleStart) >= seconds(3);
        }
        Utility::msleep(100);
    }
}
} // namespace KDC
