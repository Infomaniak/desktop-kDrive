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

#include "hydrateplaceholderjob.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

HydratePlaceHolderJob::HydratePlaceHolderJob(std::shared_ptr<SyncPal> syncPal, SyncPath absoluteLocalPath, SyncPath relativePath,
                                             SyncPath parentFolderPath /* = {} */,
                                             std::function<void(UniqueId, bool)> observer /* = nullptr */) :
    _syncPal(std::move(syncPal)),
    _absoluteLocalPath(std::move(absoluteLocalPath)),
    _relativePath(std::move(relativePath)),
    _parentFolderPath(std::move(parentFolderPath)),
    _observer(std::move(observer)) {}

ExitInfo HydratePlaceHolderJob::canRun() {
    if (bypassCheck()) return ExitCode::Ok;

    // Check that the sync and its VFS are still available.
    if (!_syncPal) {
        LOGW_WARN(_logger,
                  L"Error in HydratePlaceHolderJob::canRun - Invalid SyncPal - " << Utility::formatSyncPath(_absoluteLocalPath));
        return {ExitCode::LogicError, ExitCause::InvalidArgument};
    }

    if (!_syncPal->vfs()) {
        LOGW_WARN(_logger,
                  L"Error in HydratePlaceHolderJob::canRun - Vfs not found - " << Utility::formatSyncPath(_absoluteLocalPath));
        return ExitCode::SyncPaused;
    }

    // Check that the file still exists locally
    bool exists = false;
    if (IoError ioError = IoError::Success;
        !IoHelper::checkIfPathExists(_absoluteLocalPath, exists, ioError, IoHelper::PathCheckOption::Insensitive) ||
        ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_absoluteLocalPath, ioError));

        return IoHelper::toExitInfo(ioError, {ExitCode::SystemError, ExitCause::FileAccessError});
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"Item does not exist - " << Utility::formatSyncPath(_absoluteLocalPath));

        return {ExitCode::SystemError, ExitCause::NotFound};
    }

    return ExitCode::Ok;
}

ExitInfo HydratePlaceHolderJob::runJob() {
    if (const auto exitInfo = canRun(); !exitInfo) {
        return exitInfo;
    }

    // Check the item type
    ItemType itemType;
    if (!IoHelper::getItemTypeAndProcessIoError(_absoluteLocalPath, itemType) || itemType.ioError != IoError::Success) {
        LOGW_WARN(_logger, L"No file data - " << Utility::formatSyncPath(_absoluteLocalPath));

        return ExitCode::Ok;
    }

    if (itemType.linkType != LinkType::None) {
        LOGW_DEBUG(_logger, L"Do not hydrate symlinks - " << Utility::formatSyncPath(_absoluteLocalPath));
        if (const auto cancelHydrateExitInfo = cancelHydrate(PinState::OnlineOnly, ExitCode::Unknown); !cancelHydrateExitInfo) {
            LOGW_INFO(_logger, L"Error in HydratePlaceHolderJob::cancelHydrate - " << Utility::formatSyncPath(_absoluteLocalPath)
                                                                                   << L": " << cancelHydrateExitInfo);
        }

        return ExitCode::Ok;
    }

    // Check file status
    auto status = SyncFileStatus::Unknown;
    VfsStatus vfsStatus;
    if (ExitInfo exitInfo = syncFileStatus(status, vfsStatus); !exitInfo) {
        LOGW_WARN(_logger, L"Error in HydratePlaceHolderJob::syncFileStatus - " << Utility::formatSyncPath(_absoluteLocalPath)
                                                                                << L": " << exitInfo);
        if (const auto cancelHydrateExitInfo = cancelHydrate(PinState::OnlineOnly, exitInfo); !cancelHydrateExitInfo) {
            LOGW_INFO(_logger, L"Error in HydratePlaceHolderJob::cancelHydrate - " << Utility::formatSyncPath(_absoluteLocalPath)
                                                                                   << L": " << cancelHydrateExitInfo);
        }

        return exitInfo;
    }

    if (!vfsStatus.isPlaceholder) {
        // File is not a placeholder, this should never happen
        LOGW_WARN(_logger, L"File is not a placeholder - " << Utility::formatSyncPath(_absoluteLocalPath));

        return ExitCode::Ok;
    }

    if (vfsStatus.isHydrated || status == SyncFileStatus::Syncing) {
        LOGW_INFO(_logger, L"File is already hydrated/ing - " << Utility::formatSyncPath(_absoluteLocalPath));

        return ExitCode::Ok;
    }

    PinState ps{PinState::Unknown};
#if defined(KD_MACOS)
    // Not done in Windows case: triggers a hydration
    // Get pin state
    ps = _syncPal->vfs()->pinState(_relativePath);

    if (ps != PinState::AlwaysLocal) {
        // Set pin state
        if (const auto exitInfo = _syncPal->vfs()->setPinState(_relativePath, PinState::AlwaysLocal); !exitInfo) {
            LOGW_INFO(_logger,
                      L"Error in Vfs::setPinState - " << Utility::formatSyncPath(_absoluteLocalPath) << L": " << exitInfo);
            return exitInfo;
        }
    }
#else
    ps = PinState::OnlineOnly;
#endif

    // Create download job
    if (ExitInfo exitInfo = _syncPal->addDlDirectJob(_relativePath, _absoluteLocalPath, _parentFolderPath, _observer);
        !exitInfo) {
        LOGW_INFO(_logger,
                  L"Error in SyncPal::addDlDirectJob - " << Utility::formatSyncPath(_absoluteLocalPath) << L": " << exitInfo);

        if (const auto cancelHydrateExitInfo = cancelHydrate(ps, exitInfo); !cancelHydrateExitInfo) {
            LOGW_INFO(_logger, L"Error in HydratePlaceHolderJob::cancelHydrate - " << Utility::formatSyncPath(_absoluteLocalPath)
                                                                                   << L": " << cancelHydrateExitInfo);
        }

        return exitInfo;
    }

    // Record that a download job has been enqueued, so that callers can tell "nothing to download" apart from
    // "download in progress" once the job has run.
    _downloadQueued = true;

    return ExitCode::Ok;
}

ExitInfo HydratePlaceHolderJob::syncFileStatus(SyncFileStatus &status, VfsStatus &vfsStatus) {
    status = SyncFileStatus::Unknown;
    vfsStatus.isPlaceholder = false;
    vfsStatus.isHydrated = false;

    bool exists = false;
    if (ExitInfo exitInfo = _syncPal->checkIfExistsOnServer(_relativePath, exists); !exitInfo) {
        LOGW_DEBUG(_logger,
                   L"Error in SyncPal::checkIfExistsOnServer: " << Utility::formatSyncPath(_relativePath) << L": " << exitInfo);
        // Occurs when the sync is stopped

        return exitInfo;
    }

    if (exists) status = SyncFileStatus::Success;

    if (const auto vfs = _syncPal->vfs(); vfs->mode() == VirtualFileMode::Mac || vfs->mode() == VirtualFileMode::Win) {
        if (ExitInfo exitInfo = vfs->status(_absoluteLocalPath, vfsStatus); !exitInfo) {
            LOGW_WARN(_logger, L"Error in Vfs::status - " << Utility::formatSyncPath(_absoluteLocalPath));

            return exitInfo;
        }

        if (vfsStatus.isSyncing) status = SyncFileStatus::Syncing;
    }

    return ExitCode::Ok;
}

ExitInfo HydratePlaceHolderJob::cancelHydrate(PinState pinState, ExitInfo exitInfo) {
    _syncPal->vfs()->cancelHydrate(_absoluteLocalPath, exitInfo);

    if (pinState != PinState::Unknown) return _syncPal->vfs()->setPinState(_relativePath, pinState);

    return ExitCode::Ok;
}

} // namespace KDC
