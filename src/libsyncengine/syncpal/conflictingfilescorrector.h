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

#include "jobs/abstractpropagatorjob.h"
#include "syncpal/syncpal.h"
#include "db/error.h"

#include <vector>

namespace KDC {

class ConflictingFilesCorrector : public AbstractPropagatorJob {
    public:
        ConflictingFilesCorrector(std::shared_ptr<SyncPal> syncPal, const std::vector<Error> &keepLocalErrorList,
                                  const std::vector<Error> &keepRemoteErrorList);

        ExitInfo runJob() override;

        uint64_t nbErrors() const { return _nbErrors; }
        std::vector<ErrorDbId> removedErrorsDbIds() const { return _removedErrorsDbIds; }


    private:
        ExitInfo resolveConflicts(const std::vector<Error> &errorList, ConflictResolutionStrategy strategy);
        bool keepLocalVersion(const Error &error);
        bool keepRemoteVersion(const Error &error);
        void deleteError(ErrorDbId errorDbId);

        std::shared_ptr<SyncPal> _syncPal = nullptr;
        const std::vector<Error> _keepLocalErrors;
        const std::vector<Error> _keepRemoteErrors;
        std::vector<ErrorDbId> _removedErrorsDbIds;
        uint64_t _nbErrors = 0;
};

} // namespace KDC
