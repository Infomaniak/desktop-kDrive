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

#pragma once

#include "testhelpers.h"

#include "localtemporarydirectory.h"
#include "io/iohelper.h"

#include "libcommon/utility/utility.h"

#include "libsyncengine/jobs/network/kDrive_API/createdirjob.h"
#include "libsyncengine/jobs/network/kDrive_API/deletejob.h"
#include "libsyncengine/jobs/network/kDrive_API/duplicatejob.h"
#include "libsyncengine/jobs/network/kDrive_API/movejob.h"
#include "libsyncengine/jobs/network/kDrive_API/upload/uploadjob.h"

namespace KDC::testhelpers {

NodeId createRemoteDir(const int driveDbId, const NodeId &remoteParentId, const SyncName &name) {
    CreateDirJob job(nullptr, driveDbId, remoteParentId, name);
    (void) job.runSynchronously();
    return job.nodeId();
}

void editRemoteFile(const int driveDbId, const NodeId &remoteFileId, SyncTime *creationTime /*= nullptr*/,
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

void moveRemoteItem(const int driveDbId, const NodeId &remoteFileId, const NodeId &destinationRemoteParentId,
                    const SyncName &name /*= {}*/) {
    MoveJob job(nullptr, driveDbId, {}, remoteFileId, destinationRemoteParentId, name);
    job.setBypassCheck(true);
    (void) job.runSynchronously();
}

NodeId duplicateRemoteItem(const int driveDbId, const NodeId &id, const SyncName &newName) {
    DuplicateJob job(nullptr, driveDbId, id, newName);
    (void) job.runSynchronously();
    return job.nodeId();
}

void deleteRemoteItem(const int driveDbId, const NodeId &id) {
    DeleteJob job(driveDbId, id);
    job.setBypassCheck(true);
    (void) job.runSynchronously();
}

SyncPath findLocalFileByNamePrefix(const SyncPath &parentAbsolutePath, const SyncName &namePrefix) {
    IoError ioError(IoError::Unknown);
    IoHelper::DirectoryIterator dirIt(parentAbsolutePath, false, ioError);
    bool endOfDir = false;
    DirectoryEntry entry;
    while (dirIt.next(entry, endOfDir, ioError) && !endOfDir) {
        if (CommonUtility::startsWith(entry.path().filename(), namePrefix)) return entry.path();
    }
    return {};
}

} // namespace KDC::testhelpers
