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

#include "abstracttokennetworkjob.h"

namespace KDC {

class GetFileLinkJob : public AbstractTokenNetworkJob {
    public:
        GetFileLinkJob(int driveDbId, const NodeId &nodeId);

    private:
        virtual std::string getSpecificUrl() override;
        virtual void setQueryParameters(Poco::URI &, bool &) override {}
        inline virtual ExitInfo setData() override { return ExitCode::Ok; }

        NodeId _nodeId;
};

} // namespace KDC
