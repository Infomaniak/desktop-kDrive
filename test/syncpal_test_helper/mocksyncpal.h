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

#include "syncpal/syncpal.h"
#include "syncpal/syncpalworker.h"

namespace KDC {

/**
 * @brief Test-only SyncPal subclass exposing test hooks that must not live on the production SyncPal API.
 */
class MockSyncPal : public SyncPal {
    public:
        using SyncPal::SyncPal;

        // Test-only: stops the sync loop from advancing past `step` (SyncStep::None removes the cap).
        void setMaxStep(const SyncStep step) {
            if (_syncPalWorker) _syncPalWorker->setMaxStep(step);
        }
};

} // namespace KDC
