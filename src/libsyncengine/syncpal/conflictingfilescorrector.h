/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include <db/error.h>

#include <vector>

namespace KDC {

class ConflictingFilesCorrector : public AbstractPropagatorJob {
    public:
        ConflictingFilesCorrector(std::shared_ptr<SyncPal> syncPal, bool keepLocalVersion, std::vector<Error> &errors);

        virtual void runJob() override;

        inline uint64_t nbErrors() const { return _nbErrors; }

    private:
        bool keepLocalVersion(const Error &error);
        bool keepRemoteVersion(const Error &error);
        void deleteError(int errorDbId);

        std::shared_ptr<SyncPal> _syncPal = nullptr;
        bool _keepLocalVersion = false;
        std::vector<Error> _errors;

        uint64_t _nbErrors = 0;
};

}  // namespace KDC
