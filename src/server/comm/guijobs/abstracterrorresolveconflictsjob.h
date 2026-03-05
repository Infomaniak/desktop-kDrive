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

#include "server/comm/guijobs/abstractguijob.h"
#include "libparms/db/error.h"

#include <vector>
#include <set>

namespace KDC {

class SyncPal;

class AbstractErrorResolveConflictsJob : public AbstractGuiJob {
    public:
        AbstractErrorResolveConflictsJob(std::shared_ptr<CommManager> commManager, int requestId,
                                         const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel);

    protected:
        ExitInfo serializeOutputParms() override { return ExitCode::Ok; }

        ExitInfo fetchAllErrors(std::vector<Error> &errorList);
        ExitInfo getSyncDbIdFromErrors(const std::vector<Error> &list1, int64_t &syncDbId);
        ExitInfo getSyncDbIdFromErrors(const std::vector<Error> &list1, const std::vector<Error> &list2, int64_t &syncDbId);
        ExitInfo getSyncPal(int64_t syncDbId, std::shared_ptr<SyncPal> &syncPal);
        ExitInfo fixConflictsAndNotify(const std::shared_ptr<SyncPal> &syncPal, const std::vector<Error> &keepLocalErrors,
                                       const std::vector<Error> &keepRemoteErrors);
};

} // namespace KDC
