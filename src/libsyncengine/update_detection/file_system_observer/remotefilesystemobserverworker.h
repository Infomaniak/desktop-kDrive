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

#pragma once

#include "filesystemobserverworker.h"
#include "jobs/network/networkjobsparams.h"
#include "update_detection/file_system_observer/snapshot/snapshotitem.h"

#include <Poco/JSON/Object.h>

namespace KDC {

class CsvFullFileListWithCursorJob;

class RemoteFileSystemObserverWorker : public FileSystemObserverWorker {
    public:
        RemoteFileSystemObserverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);
        ~RemoteFileSystemObserverWorker() override;

    protected:
        void execute() override;
        virtual ExitCode sendLongPoll(bool &changes);
        ExitInfo generateInitialSnapshot() override;

    private:
        ExitCode processEvents() override;
        [[nodiscard]] ReplicaSide getSnapshotType() const override { return ReplicaSide::Remote; }

        ExitInfo initWithCursor();
        ExitInfo exploreDirectory(const NodeId &nodeId);
        ExitInfo getItemsInDir(const NodeId &dirId, bool saveCursor);

        struct ActionInfo {
                ActionCode actionCode{ActionCode::ActionCodeUnknown};
                SnapshotItem snapshotItem;
                SyncName path;
        };
        ExitInfo processActions(Poco::JSON::Array::Ptr filesArray);
        ExitInfo extractActionInfo(Poco::JSON::Object::Ptr actionObj, ActionInfo &actionInfo);
        ExitInfo processAction(ActionInfo &actionInfo, std::set<NodeId, std::less<>> &movedItems);

        ExitInfo checkRightsAndUpdateItem(const NodeId &nodeId, bool &hasRights, SnapshotItem &snapshotItem);

        bool hasUnsupportedCharacters(const SyncName &name, const NodeId &nodeId, NodeType type);

        void countListingRequests();

        int _driveDbId = -1;
        std::string _cursor;
        NodeSet _blackList; // A list of user-selected folders not to be synchronized.
        int _listingFullCounter = 0;
        std::chrono::steady_clock::time_point _listingFullTimer = std::chrono::steady_clock::now();

        friend class TestRemoteFileSystemObserverWorker;
};

} // namespace KDC
