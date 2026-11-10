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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "storagescanner.h"

#include "libcommon/utility/types.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace KDC {

/**
 * Process-local measures of the synchronization roots shown by the Storage page.
 *
 * A measure is kept for the root it was taken on, so a synchronization moved to another folder is measured again. It is
 * reused until its synchronization has been active again: the cache follows the observed synchronization statuses and
 * marks a synchronization as needing a new measure while it is active. Only used on the GUI thread.
 */
class StorageMeasureCache {
    public:
        struct Entry {
                SyncPath syncRoot{};
                dev_t device{0};
                uint64_t bytes{0};
                // False for a synchronization stored on another volume, whose bytes have never been needed.
                bool measured{false};
                // Volume read with the measure of a selected synchronization, shown at once when it is selected again.
                std::optional<StorageVolume> volume{};
        };

        [[nodiscard]] static bool isActiveStatus(SyncStatus status);

        /** Entry of `syncDbId`, only when it was recorded for `syncRoot`. */
        [[nodiscard]] const Entry *find(SyncDbId syncDbId, const SyncPath &syncRoot) const;
        [[nodiscard]] bool isKnown(SyncDbId syncDbId, const SyncPath &syncRoot) const;
        [[nodiscard]] bool needsMeasure(SyncDbId syncDbId, const SyncPath &syncRoot) const;
        /**
         * Sums the measures of `syncs` stored on `device`. Returns nothing while one of them is unknown, or on `device`
         * without a measure.
         */
        [[nodiscard]] std::optional<uint64_t> bytesOnDevice(const std::vector<StorageScanTarget> &syncs, dev_t device) const;

        /** Records the measure of the selected synchronization, with the volume it was read on. */
        void recordSelectedMeasure(SyncDbId syncDbId, const SyncPath &syncRoot, const std::optional<StorageVolume> &volume,
                                   uint64_t bytes, bool syncActive);
        /**
         * Records the device and, when measured, the bytes of another synchronization. Returns its cached bytes when it is
         * stored on the scanned volume.
         */
        std::optional<uint64_t> recordOtherSync(const StorageScanUpdate &update, bool syncActive);
        void setVolume(SyncDbId syncDbId, const std::optional<StorageVolume> &volume);
        void erase(SyncDbId syncDbId);
        /** Drops everything known about the synchronizations missing from `syncDbIds`. */
        void retain(const std::unordered_set<SyncDbId> &syncDbIds);

        void markDirty(SyncDbId syncDbId);
        [[nodiscard]] bool isDirty(SyncDbId syncDbId) const;
        /**
         * Records the status of a synchronization and marks it as needing a new measure while it is active. Returns true
         * when it has just left an active or transitional status.
         */
        bool observeStatus(SyncDbId syncDbId, SyncStatus status);
        void forgetStatus(SyncDbId syncDbId);

    private:
        void clearDirtyUnlessActive(SyncDbId syncDbId, bool syncActive);

        std::unordered_map<SyncDbId, Entry> _entries;
        std::unordered_set<SyncDbId> _dirtySyncs;
        std::unordered_map<SyncDbId, SyncStatus> _observedStatuses;
};

} // namespace KDC
