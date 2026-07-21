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

#include "initialsituationsetter.h"
#include "operationsexecutor.h"

#include <memory>

namespace KDC {
class SyncPal;

/**
 * @brief Single entry point for setting up and driving Syncpal-based tests: building an initial
 * Db/update-tree/filesystem situation from a JSON description (via InitialSituationSetter), applying
 * operations on top of it (via OperationsExecutor), and (eventually) driving a sync run.
 *
 * See Situation (initialsituationsetter.h) for the supported situation JSON formats, and Operations
 * (operationsexecutor.h) for the supported operations JSON format.
 */
class SyncpalTestHelper {
    public:
        SyncpalTestHelper() = default;
        explicit SyncpalTestHelper(std::shared_ptr<SyncPal> syncPal);

        // ---- High-level test driver API ----
        void setUp();
        void tearDown();

        void setSyncpal(std::shared_ptr<SyncPal> syncPal);

        // Builds localSituation and remoteSituation independently (see
        // InitialSituationSetter::generateInitialSituation) against the SyncPal passed to the constructor (or set
        // via setSyncpal). localSituation and remoteSituation may differ.
        // returns false if invalid
        bool setInitialSituation(const Situation &localSituation, const Situation &remoteSituation);
        bool getSituation(const Situation &localSituation, const Situation &remoteSituation) const;

        bool executeSyncUntilEnd(const std::chrono::milliseconds minWaitTime = std::chrono::milliseconds(3000)) const;
        bool executeSyncUpToStep(const int64_t targetStep, const int64_t timeout) const;

        bool pauseSync() const;
        bool stopSync() const;

        // Applies operations (see OperationsExecutor::execute) on the given side, against the
        // SyncPal passed to the constructor (or set via setSyncpal).
        // returns false if invalid
        bool execute(ReplicaSide side, const Operations &operations);

    private:
        std::shared_ptr<SyncPal> _syncPal;

        InitialSituationSetter _setInitialSituation;
        OperationsExecutor _executeOperations;
};

} // namespace KDC
