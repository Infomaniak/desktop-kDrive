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

#include "remotefolderprovider.h"

#include "app/services/commservice.h"

namespace KDC {

CommRemoteFolderProvider::CommRemoteFolderProvider(CommService &commService) :
    _commService(commService) {}

void CommRemoteFolderProvider::requestNodeInfo(const UserDbId userDbId, const DriveId driveId, const NodeId &nodeId,
                                               const NodeInfoCallback &callback) const {
    _commService.requestNodeInfo(userDbId, driveId, nodeId, true, callback);
}

void CommRemoteFolderProvider::requestChildren(const UserDbId userDbId, const DriveId driveId, const NodeId &nodeId,
                                               const ChildrenCallback &callback) const {
    _commService.requestNodeSubfolders(userDbId, driveId, nodeId, true, callback);
}

void CommRemoteFolderProvider::requestSize(const UserDbId userDbId, const DriveId driveId, const NodeId &nodeId,
                                           const SizeCallback &callback) const {
    _commService.requestNodeFolderSize(userDbId, driveId, nodeId, callback);
}

} // namespace KDC
