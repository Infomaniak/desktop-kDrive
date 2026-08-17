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

#include "libcommon/info/syncfileiteminfo.h"

#include <QDateTime>
#include <QObject>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace KDC {

/** @brief Process-local representation of one file synchronization activity. */
struct ActivityEntry {
        GenericId localId{0};
        SyncDbId syncDbId{0};
        UniqueId operationId{0};
        NodeType nodeType{NodeType::Unknown};
        SyncPath path;
        SyncPath newPath;
        QString localNodeId;
        QString remoteNodeId;
        SyncDirection direction{SyncDirection::Unknown};
        SyncFileInstruction instruction{SyncFileInstruction::None};
        SyncFileStatus status{SyncFileStatus::Unknown};
        int64_t size{0};
        int32_t progress{0};
        QDateTime receivedAtUtc;
        Count receivedSequence{0};
};

/**
 * @brief Process-local, bounded history of file synchronization activities for Linux.
 *
 * The store retains validated server DTO data and at most maxActivitiesPerSync entries for each synchronization. It is
 * intentionally separate from AppCache's durable entity graph and must only be mutated from its QObject thread.
 */
class ActivityStore final : public QObject {
        Q_OBJECT

    public:
        /** @brief Maximum number of retained activities for each synchronization. */
        static constexpr std::size_t maxActivitiesPerSync = 500;

        /**
         * @brief Creates an empty activity store.
         * @param parent Optional QObject owner.
         */
        explicit ActivityStore(QObject *parent = nullptr);

        /**
         * @brief Inserts a new server activity or updates the matching in-progress operation.
         *
         * Successful and in-progress activities remove superseded failed entries that share a non-empty local or remote
         * node identifier.
         * @param syncDbId Database identifier of the owning synchronization.
         * @param item Activity DTO received from the server.
         */
        void ingest(SyncDbId syncDbId, const SyncFileItemInfo &item);

        /**
         * @brief Returns the retained activities for one synchronization.
         * @param syncDbId Database identifier of the synchronization to query.
         * @return A snapshot copy of the retained activities, or an empty vector when the synchronization has none.
         */
        [[nodiscard]] std::vector<ActivityEntry> activities(SyncDbId syncDbId) const;

        /**
         * @brief Removes every in-progress activity owned by one synchronization.
         * @param syncDbId Database identifier of the synchronization whose interrupted activities must be removed.
         */
        void removeInProgress(SyncDbId syncDbId);

        /**
         * @brief Removes every retained activity owned by one synchronization.
         * @param syncDbId Database identifier of the synchronization to remove.
         */
        void removeSync(SyncDbId syncDbId);

        /**
         * @brief Removes activities whose owning synchronization is absent from the supplied set.
         * @param syncDbIds Database identifiers of all synchronizations that must be retained.
         */
        void retainSyncs(const std::unordered_set<SyncDbId> &syncDbIds);

        /** @brief Removes every retained activity from the store. */
        void clear();

    signals:
        /**
         * @brief Notifies consumers that one synchronization's activity snapshot changed.
         * @param syncDbId Database identifier of the changed synchronization.
         */
        void activitiesChanged(SyncDbId syncDbId);

    private:
        [[nodiscard]] ActivityEntry makeEntry(SyncDbId syncDbId, const SyncFileItemInfo &item, GenericId localId);
        static void enforceCapacity(std::vector<ActivityEntry> &entries);

        std::unordered_map<SyncDbId, std::vector<ActivityEntry>> _activitiesBySyncDbId;
        GenericId _nextLocalId{1};
        Count _nextReceivedSequence{1};
};

} // namespace KDC
