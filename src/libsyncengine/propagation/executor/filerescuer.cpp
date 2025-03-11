// Infomaniak kDrive - Desktop
// Copyright (C) 2023-2025 Infomaniak Network SA
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "filerescuer.h"

#include "jobs/local/localcreatedirjob.h"
#include "jobs/local/localmovejob.h"

#include <asserts.h>

namespace KDC {

const SyncPath rescueFolderName = ".rescueFolder";

ExitInfo FileRescuer::executeRescueMoveJob(const SyncOpPtr syncOp) const {
    if (const auto exitInfo = createRescueFolderIfNeeded(); !exitInfo) {
        return exitInfo;
    }

    const auto absoluteOriginPath = _syncPal->localPath() / syncOp->relativeOriginPath();
    SyncPath relativeDestinationPath;
    ExitInfo exitInfo;
    auto counter = 0;
    SyncName suffix;
    do {
        if (counter) {
            suffix = Str(" (") + Str2SyncName(std::to_string(counter)) + Str(")"); // TODO : use format when fully moved to c++20
        }
        const SyncName filename = Str2SyncName(syncOp->relativeOriginPath().stem().string()) + suffix +
                                  Str2SyncName(syncOp->relativeOriginPath().extension().string());
        relativeDestinationPath = rescueFolderName / filename;
        LocalMoveJob rescueJob(absoluteOriginPath, _syncPal->localPath() / relativeDestinationPath);
        exitInfo = rescueJob.runSynchronously();
        if (exitInfo.cause() == ExitCause::FileAlreadyExist) {
            ++counter;
        } else if (!exitInfo) {
            return exitInfo;
        }
    } while (!exitInfo);

    LOG_IF_FAIL(syncOp->conflict().localNode() && syncOp->conflict().localNode()->id().has_value())
    const auto localNodeId = syncOp->conflict().localNode()->id().value();
    const Error error(_syncPal->syncDbId(), localNodeId, {}, syncOp->conflict().localNode()->type(), syncOp->relativeOriginPath(),
                      ConflictType::None, InconsistencyType::None, CancelType::FileRescued, relativeDestinationPath);
    _syncPal->addError(error);

    return ExitCode::Ok;
}

ExitInfo FileRescuer::createRescueFolderIfNeeded() const {
    const auto rescueFolderPath = _syncPal->localPath() / rescueFolderName;

    bool exists = false;
    auto ioError = IoError::Unknown;
    (void) IoHelper::checkIfPathExists(rescueFolderPath, exists, ioError);
    if (ioError != IoError::Success) {
        // Failed to check directory existence
        LOGW_WARN(KDC::Log::instance()->getLogger(),
                  L"Failed to check rescue directory existence. Error code: " << Utility::formatIoError(ioError));
        return ExitCode::SystemError;
    }
    if (exists) {
        // Rescue folder already exists
        return ExitCode::Ok;
    }

    LocalCreateDirJob job(rescueFolderPath);
    if (const auto exitInfo = job.runSynchronously(); !exitInfo) {
        return exitInfo;
    }
    return ExitCode::Ok;
}

} // namespace KDC
