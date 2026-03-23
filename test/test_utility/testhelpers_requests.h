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

NodeId createRemoteDir(const int driveDbId, const NodeId &remoteParentId, const SyncName &name);
void editRemoteFile(const int driveDbId, const NodeId &remoteFileId, SyncTime *creationTime = nullptr,
                    SyncTime *modificationTime = nullptr, int64_t *size = nullptr);
void moveRemoteItem(const int driveDbId, const NodeId &remoteFileId, const NodeId &destinationRemoteParentId,
                    const SyncName &name = {});
NodeId duplicateRemoteItem(const int driveDbId, const NodeId &id, const SyncName &newName);
void deleteRemoteItem(const int driveDbId, const NodeId &id);
SyncPath findLocalFileByNamePrefix(const SyncPath &parentAbsolutePath, const SyncName &namePrefix);

} // namespace KDC::testhelpers
