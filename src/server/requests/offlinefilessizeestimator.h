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

#include "syncpal/syncpal.h"

namespace KDC {

class OfflineFilesSizeEstimator {
    public:
        explicit OfflineFilesSizeEstimator(const std::vector<std::shared_ptr<SyncPal>> &syncPals);
        ExitInfo runSynchronously();

        [[nodiscard]] uint64_t offlineFilesTotalSize() const { return _offlineFilesTotalSize; }

    private:
        std::vector<std::shared_ptr<SyncPal>> _syncPals;
        uint64_t _offlineFilesTotalSize{0};
};

} // namespace KDC
