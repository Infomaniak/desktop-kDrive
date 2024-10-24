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

#include "conflictingfilescorrector.h"

#include "jobs/local/localdeletejob.h"
#include "jobs/local/localmovejob.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h" // Path2WStr

#include <log4cplus/loggingmacros.h>

#include <Poco/Timestamp.h>
#include <Poco/File.h>
#include <Poco/Timestamp.h>

namespace KDC {

ConflictingFilesCorrector::ConflictingFilesCorrector(std::shared_ptr<SyncPal> syncPal, bool keepLocalVersion,
                                                     std::vector<Error> &errors) :
    _syncPal(syncPal), _keepLocalVersion(keepLocalVersion), _errors(std::move(errors)) {}

void ConflictingFilesCorrector::runJob() {
    for (auto &error: _errors) {
        bool exists = false;
        IoError ioError = IoError::Success;
        if (!IoHelper::checkIfPathExists(error.destinationPath(), exists, ioError)) {
            LOGW_WARN(Log::instance()->getLogger(), L"Error in IoHelper::checkIfPathExists: "
                                                            << Utility::formatIoError(error.destinationPath(), ioError).c_str());
            _nbErrors++;
            continue;
        }

        if (!exists) {
            // Ok, conflict already solved
            deleteError(error.dbId());
            continue;
        }

        if (_keepLocalVersion) {
            if (keepLocalVersion(error)) {
                deleteError(error.dbId());
            } else {
                _nbErrors++;
            }
        } else {
            if (keepRemoteVersion(error)) {
                deleteError(error.dbId());
            } else {
                _nbErrors++;
            }
        }
    }

    _exitCode = ExitCode::Ok;
}

bool ConflictingFilesCorrector::keepLocalVersion(const Error &error) {
    // Delete remote version locally
    SyncPath originalAbsolutePath = error.destinationPath().parent_path() / error.path().filename();
    LocalDeleteJob deleteJob(originalAbsolutePath);
    deleteJob.runSynchronously();
    if (deleteJob.exitCode() != ExitCode::Ok) {
        return false;
    }

    // Rename the local version
    LocalMoveJob renameJob(error.destinationPath(), originalAbsolutePath);
    renameJob.runSynchronously();
    if (renameJob.exitCode() != ExitCode::Ok) {
        return false;
    }

    // Set the local modification time to now
    Poco::Timestamp lastModifiedTimestamp;
    Poco::File(Path2Str(originalAbsolutePath)).setLastModified(lastModifiedTimestamp);

    return true;
}

bool ConflictingFilesCorrector::keepRemoteVersion(const Error &error) {
    // Delete local version
    LocalDeleteJob deleteJob(error.destinationPath());
    deleteJob.runSynchronously();
    if (deleteJob.exitCode() != ExitCode::Ok) {
        return false;
    }

    return true;
}

void ConflictingFilesCorrector::deleteError(int errorDbId) {
    bool found = false;
    ParmsDb::instance()->deleteError(errorDbId, found);
}

} // namespace KDC
