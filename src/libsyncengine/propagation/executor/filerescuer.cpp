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

#include "filerescuer.h"

#include "jobs/local/localcreatedirjob.h"
#include "jobs/local/localmovejob.h"
#include "utility/logiffail.h"

namespace KDC {

const SyncPath FileRescuer::_rescueFolderName = "kDrive Rescue Folder";

ExitInfo FileRescuer::executeRescueMoveJob(const SyncOpPtr syncOp) {
    if (const auto exitInfo = createRescueFolderIfNeeded(); !exitInfo) {
        return exitInfo;
    }

    const auto relativeOriginPath = syncOp->relativeOriginPath();
    SyncPath relativeDestinationPath;
    if (const auto exitInfo = moveToRescueFolder(relativeOriginPath, relativeDestinationPath); !exitInfo) {
        return exitInfo;
    }

    LOG_IF_FAIL_2(Log::instance()->getLogger(),
                  syncOp->conflict().localNode() && syncOp->conflict().localNode()->id().has_value())
    const auto localNodeId = syncOp->conflict().localNode()->id().value();
    const Error error(_syncPal->syncDbId(), localNodeId, {}, syncOp->conflict().localNode()->type(), syncOp->relativeOriginPath(),
                      ConflictType::None, InconsistencyType::None, CancelType::FileRescued, relativeDestinationPath);
    _syncPal->addError(error);

    return ExitCode::Ok;
}

ExitInfo FileRescuer::createRescueFolderIfNeeded() const {
    const auto rescueFolderPath = _syncPal->localPath() / _rescueFolderName;

    bool exists = false;
    auto ioError = IoError::Unknown;
    (void) IoHelper::checkIfPathExists(rescueFolderPath, exists, ioError);
    if (ioError != IoError::Success) {
        // Failed to check directory existence
        LOGW_WARN(KDC::Log::instance()->getLogger(),
                  L"Failed to check rescue directory existence. Error: " << Utility::formatIoError(ioError));
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

ExitInfo FileRescuer::moveToRescueFolder(const SyncPath &relativeOriginPath, SyncPath &relativeDestinationPath) const {
    ExitInfo exitInfo;
    uint16_t counter = 0;
    do {
        relativeDestinationPath = getDestinationPath(relativeOriginPath, counter++);
        LocalMoveJob rescueJob(_syncPal->localPath() / relativeOriginPath, _syncPal->localPath() / relativeDestinationPath);
        exitInfo = rescueJob.runSynchronously();
    } while (!exitInfo && exitInfo.cause() == ExitCause::FileExists);
    return ExitCode::Ok;
}

SyncPath FileRescuer::getDestinationPath(const SyncPath &relativeOriginPath, const uint16_t counter /*= 0*/) const {
    if (counter == 0) return _syncPal->localPath() / _rescueFolderName / relativeOriginPath.filename();
    const SyncName suffix =
            Str(" (") + Str2SyncName(std::to_string(counter)) + Str(")"); // TODO : use format when fully moved to c++20
    const SyncName filename = relativeOriginPath.stem().native() + suffix + relativeOriginPath.extension().native();
    return _syncPal->localPath() / _rescueFolderName / filename;
}

} // namespace KDC
