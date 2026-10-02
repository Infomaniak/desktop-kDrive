/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "testconflictingfilescorrector.h"

#include "config.h"
#include "libparms/db/parmsdb.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "requests/parameterscache.h"
#include "syncpal/conflictingfilescorrector.h"
#include "test_classes/syncpaltest.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <version.h>

using namespace CppUnit;

namespace KDC {

namespace {
void createFileWithContent(const SyncPath &path, const std::string &content) {
    std::ofstream ofs(path);
    CPPUNIT_ASSERT(ofs.is_open());
    ofs << content;
}

std::string readFileContent(const SyncPath &path) {
    std::ifstream ifs(path);
    CPPUNIT_ASSERT(ifs.is_open());
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}
} // namespace

void TestConflictingFilesCorrector::setUp() {
    TestBase::start();

    // Create parmsDb with a minimal set of entities
    (void) ParmsDb::instance(_localParmsDbTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);

    User user(1, 12321, "keychainKey");
    CPPUNIT_ASSERT(ParmsDb::instance()->insertUser(user));

    Account account(1, 4321, user.dbId(), "account1");
    CPPUNIT_ASSERT(ParmsDb::instance()->insertAccount(account));

    Drive drive(1, 1234, account.dbId(), std::string(), 0, std::string());
    CPPUNIT_ASSERT(ParmsDb::instance()->insertDrive(drive));

    // Create the sync directory. The path of the temporary directory is canonical, so that the containment checks performed
    // by the corrector also succeed on macOS, where the system temporary directory is reached through a symbolic link.
    _syncDirPath = _syncTempDir.path() / "sync";
    CPPUNIT_ASSERT(std::filesystem::create_directory(_syncDirPath));

    Sync sync(1, drive.dbId(), _syncDirPath, "", "");
    sync.setDbPath(_localParmsDbTempDir.path() / MockDb::makeDbMockFileName());
    CPPUNIT_ASSERT(ParmsDb::instance()->insertSync(sync));

    _syncPal = std::make_shared<SyncPalTest>(1, KDRIVE_VERSION_STRING);
    _syncPal->syncDb()->setAutoDelete(true);

    // Delete items without moving them to the trash, for a deterministic behavior on every platform.
    ParametersCache::instance()->parameters().setMoveToTrash(false);
}

void TestConflictingFilesCorrector::tearDown() {
    ParametersCache::reset();

    if (_syncPal && _syncPal->syncDb()) _syncPal->syncDb()->close();

    _syncPal.reset();

    ParmsDb::instance()->close();
    ParmsDb::reset();

    TestBase::stop();
}

Error TestConflictingFilesCorrector::makeConflictError(const SyncPath &relativePath, const SyncPath &destinationPath) {
    Error error(1 /*syncDbId*/, "localNodeId", "remoteNodeId", NodeType::File, relativePath, ConflictType::EditEdit,
                InconsistencyType::None, CancelType::None, destinationPath);
    CPPUNIT_ASSERT(ParmsDb::instance()->insertError(error));

    return error;
}

void TestConflictingFilesCorrector::checkErrorInParmsDb(const ErrorDbId errorDbId, const bool expectedFound) {
    Error error;
    bool found = !expectedFound;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectError(errorDbId, error, found));
    CPPUNIT_ASSERT_EQUAL(expectedFound, found);
}

void TestConflictingFilesCorrector::testKeepLocalResolution() {
    // The local replica contains the remote version of the file and a conflicting copy of the local version.
    const SyncPath remoteVersionPath = _syncDirPath / "file.txt";
    const SyncPath localVersionPath = _syncDirPath / "file_conflicting_copy.txt";
    createFileWithContent(remoteVersionPath, "remote version");
    createFileWithContent(localVersionPath, "local version");

    const Error error = makeConflictError("file.txt", "file_conflicting_copy.txt");
    const ErrorDbId errorDbId = error.dbId();

    ConflictingFilesCorrector corrector(_syncPal, {error}, {});
    CPPUNIT_ASSERT(corrector.runJob());

    // The remote version has been deleted and the local version has been renamed into its place.
    CPPUNIT_ASSERT(!std::filesystem::exists(localVersionPath));
    CPPUNIT_ASSERT(std::filesystem::exists(remoteVersionPath));
    CPPUNIT_ASSERT_EQUAL(std::string("local version"), readFileContent(remoteVersionPath));

    // The error has been solved without any failure.
    CPPUNIT_ASSERT_EQUAL(uint64_t{0}, corrector.nbErrors());
    CPPUNIT_ASSERT_EQUAL(size_t{1}, corrector.removedErrorsDbIds().size());
    CPPUNIT_ASSERT_EQUAL(errorDbId, corrector.removedErrorsDbIds().front());
    checkErrorInParmsDb(errorDbId, false);
}

void TestConflictingFilesCorrector::testKeepRemoteResolution() {
    // The local replica contains the remote version of the file and a conflicting copy of the local version.
    const SyncPath remoteVersionPath = _syncDirPath / "file.txt";
    const SyncPath localVersionPath = _syncDirPath / "file_conflicting_copy.txt";
    createFileWithContent(remoteVersionPath, "remote version");
    createFileWithContent(localVersionPath, "local version");

    const Error error = makeConflictError("file.txt", "file_conflicting_copy.txt");
    const ErrorDbId errorDbId = error.dbId();

    ConflictingFilesCorrector corrector(_syncPal, {}, {error});
    CPPUNIT_ASSERT(corrector.runJob());

    // The local conflicting copy has been deleted and the remote version has been left untouched.
    CPPUNIT_ASSERT(!std::filesystem::exists(localVersionPath));
    CPPUNIT_ASSERT(std::filesystem::exists(remoteVersionPath));
    CPPUNIT_ASSERT_EQUAL(std::string("remote version"), readFileContent(remoteVersionPath));

    // The error has been solved without any failure.
    CPPUNIT_ASSERT_EQUAL(uint64_t{0}, corrector.nbErrors());
    CPPUNIT_ASSERT_EQUAL(size_t{1}, corrector.removedErrorsDbIds().size());
    CPPUNIT_ASSERT_EQUAL(errorDbId, corrector.removedErrorsDbIds().front());
    checkErrorInParmsDb(errorDbId, false);
}

void TestConflictingFilesCorrector::testRejectInvalidDotComponentDestinationPaths() {
    // A destination path reduced to a dot component is rejected by the upfront path validation.
    {
        const Error error = makeConflictError("file.txt", ".");
        const ErrorDbId errorDbId = error.dbId();

        ConflictingFilesCorrector corrector(_syncPal, {}, {error});
        CPPUNIT_ASSERT(corrector.runJob());

        CPPUNIT_ASSERT_EQUAL(uint64_t{1}, corrector.nbErrors());
        CPPUNIT_ASSERT(corrector.removedErrorsDbIds().empty());
        checkErrorInParmsDb(errorDbId, true);
    }

    // A destination path reduced to a dot-dot component is rejected by the upfront path validation. The item that it refers
    // to, located outside the sync directory, must not be affected.
    {
        const SyncPath outsideFilePath = _syncTempDir.path() / "outside_file.txt";
        createFileWithContent(outsideFilePath, "outside content");

        const Error error = makeConflictError("file.txt", "..");
        const ErrorDbId errorDbId = error.dbId();

        ConflictingFilesCorrector corrector(_syncPal, {}, {error});
        CPPUNIT_ASSERT(corrector.runJob());

        CPPUNIT_ASSERT_EQUAL(std::string("outside content"), readFileContent(outsideFilePath));
        CPPUNIT_ASSERT_EQUAL(uint64_t{1}, corrector.nbErrors());
        CPPUNIT_ASSERT(corrector.removedErrorsDbIds().empty());
        checkErrorInParmsDb(errorDbId, true);
    }

    // A destination path escaping the sync directory with a dot-dot component is rejected by the canonical parent directory
    // containment check. The item that it refers to, located outside the sync directory, must not be deleted.
    {
        const SyncPath outsideFilePath = _syncTempDir.path() / "outside_file_2.txt";
        createFileWithContent(outsideFilePath, "outside content");

        const Error error = makeConflictError("file.txt", "../outside_file_2.txt");
        const ErrorDbId errorDbId = error.dbId();

        ConflictingFilesCorrector corrector(_syncPal, {}, {error});
        CPPUNIT_ASSERT(corrector.runJob());

        CPPUNIT_ASSERT_EQUAL(std::string("outside content"), readFileContent(outsideFilePath));
        CPPUNIT_ASSERT_EQUAL(uint64_t{1}, corrector.nbErrors());
        CPPUNIT_ASSERT(corrector.removedErrorsDbIds().empty());
        checkErrorInParmsDb(errorDbId, true);
    }
}

void TestConflictingFilesCorrector::testKeepRemoteVersionWithSymlinkedParentPath() {
    // Create a directory outside the sync directory containing the item to be deleted, and a symbolic link, inside the
    // sync directory, pointing to this directory.
    const SyncPath externalDirPath = _syncTempDir.path() / "external_dir";
    CPPUNIT_ASSERT(std::filesystem::create_directory(externalDirPath));
    const SyncPath victimFilePath = externalDirPath / "victim.txt";
    createFileWithContent(victimFilePath, "victim content");

    const SyncPath escapeDirPath = _syncDirPath / "escape_dir";
    std::filesystem::create_directory_symlink(externalDirPath, escapeDirPath);

    const Error error = makeConflictError(SyncPath(), "escape_dir/victim.txt");
    const ErrorDbId errorDbId = error.dbId();

    ConflictingFilesCorrector corrector(_syncPal, {}, {error});
    CPPUNIT_ASSERT(corrector.runJob());

    // The canonical parent directory of the destination path is located outside of the sync directory: the victim file must
    // not have been deleted.
    CPPUNIT_ASSERT_EQUAL(std::string("victim content"), readFileContent(victimFilePath));
    CPPUNIT_ASSERT_EQUAL(uint64_t{1}, corrector.nbErrors());
    CPPUNIT_ASSERT(corrector.removedErrorsDbIds().empty());
    checkErrorInParmsDb(errorDbId, true);
}

void TestConflictingFilesCorrector::testKeepLocalVersionWithSymlinkedParentPaths() {
    // Create a directory outside the sync directory containing both the remote version of the file and a conflicting copy
    // of the local version, and a symbolic link, inside the sync directory, pointing to this directory.
    const SyncPath externalDirPath = _syncTempDir.path() / "external_dir";
    CPPUNIT_ASSERT(std::filesystem::create_directory(externalDirPath));
    const SyncPath victimFilePath = externalDirPath / "victim.txt";
    createFileWithContent(victimFilePath, "remote version");
    const SyncPath conflictCopyFilePath = externalDirPath / "conflict_copy.txt";
    createFileWithContent(conflictCopyFilePath, "local version");

    const SyncPath escapeDirPath = _syncDirPath / "escape_dir";
    std::filesystem::create_directory_symlink(externalDirPath, escapeDirPath);

    const Error error = makeConflictError("escape_dir/victim.txt", "escape_dir/conflict_copy.txt");
    const ErrorDbId errorDbId = error.dbId();

    ConflictingFilesCorrector corrector(_syncPal, {error}, {});
    CPPUNIT_ASSERT(corrector.runJob());

    // The canonical parent directories of the source and destination paths are located outside of the sync directory: neither
    // the victim file nor the conflicting copy must have been deleted or renamed.
    CPPUNIT_ASSERT_EQUAL(std::string("remote version"), readFileContent(victimFilePath));
    CPPUNIT_ASSERT_EQUAL(std::string("local version"), readFileContent(conflictCopyFilePath));
    CPPUNIT_ASSERT_EQUAL(uint64_t{1}, corrector.nbErrors());
    CPPUNIT_ASSERT(corrector.removedErrorsDbIds().empty());
    checkErrorInParmsDb(errorDbId, true);
}
} // namespace KDC
