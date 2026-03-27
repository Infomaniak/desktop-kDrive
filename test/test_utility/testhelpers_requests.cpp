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

#include "testhelpers_requests.h"

#include "jobs/network/kDrive_API/renamejob.h"

namespace KDC::testhelpers {

NodeId createRemoteDir(const DriveDbId driveDbId, const NodeId &remoteParentId, const SyncName &name) {
    CreateDirJob job(nullptr, driveDbId, remoteParentId, name);
    (void) job.runSynchronously();
    return job.nodeId();
}

void editRemoteFile(const DriveDbId driveDbId, const NodeId &remoteFileId, SyncTime *creationTime /*= nullptr*/,
                    SyncTime *modificationTime /*= nullptr*/, int64_t *size /*= nullptr*/) {
    const LocalTemporaryDirectory temporaryDir;
    const auto tmpFilePath = temporaryDir.path() / ("tmpFile_" + CommonUtility::generateRandomStringAlphaNum(10));
    testhelpers::generateOrEditTestFile(tmpFilePath);

    const auto timestamp = duration_cast<std::chrono::seconds>(
            time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch());
    UploadJob job(nullptr, driveDbId, tmpFilePath, remoteFileId, timestamp.count());
    (void) job.runSynchronously();
    if (creationTime) {
        *creationTime = job.creationTime();
    }
    if (modificationTime) {
        *modificationTime = job.modificationTime();
    }
    if (size) {
        *size = job.size();
    }
}

void moveRemoteItem(const DriveDbId driveDbId, const NodeId &remoteFileId, const NodeId &destinationRemoteParentId,
                    const SyncName &name /*= {}*/) {
    MoveJob job(nullptr, driveDbId, {}, remoteFileId, destinationRemoteParentId, name);
    job.setBypassCheck(true);
    (void) job.runSynchronously();
}

void renameRemoteItem(const DriveDbId driveDbId, const NodeId &remoteFileId, const SyncName &name) {
    RenameJob job(nullptr, driveDbId, remoteFileId, name);
    job.setBypassCheck(true);
    (void) job.runSynchronously();
}

NodeId duplicateRemoteItem(const DriveDbId driveDbId, const NodeId &id, const SyncName &newName) {
    DuplicateJob job(nullptr, driveDbId, id, newName);
    (void) job.runSynchronously();
    return job.nodeId();
}

void deleteRemoteItem(const DriveDbId driveDbId, const NodeId &id) {
    DeleteJob job(driveDbId, id);
    job.setBypassCheck(true);
    (void) job.runSynchronously();
}

} // namespace KDC::testhelpers
