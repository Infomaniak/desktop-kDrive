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

#include "genericlocaldeletejob.h"

namespace KDC {

GenericLocalDeleteJob::GenericLocalDeleteJob(const SyncPath &absolutePath, bool forceHardDelete /*= false*/) :
    _absolutePath(absolutePath),
    _forceHardDelete(forceHardDelete) {}

ExitInfo GenericLocalDeleteJob::runJob() {
    if (!_forceHardDelete && ParametersCache::instance()->parameters().moveToTrash())
        return moveToTrashOrHardDeleteIfNeeded(_absolutePath);
    return hardDelete(absolutePath());
}

ExitInfo GenericLocalDeleteJob::moveToTrashOrHardDeleteIfNeeded(const SyncPath &path) {
    if (const bool moveToTrashSuccess = IoHelper::moveItemToTrash(path); !moveToTrashSuccess) {
        LOGW_WARN(_logger, L"Failed to move item: " << Utility::formatSyncPath(path) << L" to trash. Trying hard delete.");
        return hardDelete(path);
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_DEBUG(_logger, L"Item with " << Utility::formatSyncPath(path) << L" was moved to trash.");
    }

    return ExitCode::Ok;
}

ExitInfo GenericLocalDeleteJob::hardDelete(const SyncPath &path) {
    LOGW_DEBUG(_logger, L"Try to hard delete item with " << Utility::formatSyncPath(path));

    std::error_code ec;
    (void) std::filesystem::remove_all(path, ec);
    if (ec) {
        LOGW_WARN(_logger, L"Failed to delete item with " << Utility::formatStdError(_absolutePath, ec));
        if (IoHelper::stdError2ioError(ec) == IoError::AccessDenied) {
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        }

        return ExitCode::SystemError;
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_INFO(_logger, L"Item: " << Utility::formatSyncPath(path) << L" deleted.");
    }

    return ExitCode::Ok;
}

} // namespace KDC
