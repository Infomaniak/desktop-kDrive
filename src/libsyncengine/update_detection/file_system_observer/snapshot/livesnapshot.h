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

#include "syncpal/sharedobject.h"
#include "snapshot.h"
#include "snapshotrevisionhandler.h"
#include "db/dbnode.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>

namespace KDC {

class LiveSnapshot : public Snapshot, public SharedObject {
    public:
        LiveSnapshot(ReplicaSide side, const DbNode &dbNode);

        LiveSnapshot(LiveSnapshot const &) = delete;
        LiveSnapshot &operator=(const LiveSnapshot &other) = delete;

        std::unique_ptr<ConstSnapshot> getConstCopy();
        void init();

        bool updateItem(const SnapshotItem &newItem);
        bool removeItem(const NodeId itemId); // Do not pass by reference to avoid dangling references

        bool path(const NodeId &itemId, SyncPath &path, bool &ignore) const noexcept override;
        bool setName(const NodeId &itemId, const SyncName &newName);
        bool setCreatedAt(const NodeId &itemId, SyncTime newTime);
        bool setLastModified(const NodeId &itemId, SyncTime newTime);
        bool setContentChecksum(const NodeId &itemId, const std::string &newChecksum);
        bool clearContentChecksum(const NodeId &itemId);

        bool isValid() const;
        void setValid(bool newIsValid);
        SnapshotRevision revision() const override;

    private:
        std::shared_ptr<SnapshotRevisionHandler> _revisionHandlder;
        bool removeItem(std::shared_ptr<SnapshotItem> &item);
        void removeChildrenRecursively(const std::shared_ptr<SnapshotItem> &parent);
        bool _isValid = false;

        friend class TestSnapshot;
};

} // namespace KDC
