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

#include "jobs/network/abstracttokennetworkjob.h"

namespace KDC {

class AbstractListingJob : public AbstractTokenNetworkJob {
    public:
        explicit AbstractListingJob(int driveDbId, const NodeSet &blacklist = {});
        explicit AbstractListingJob(ApiType apiType, int driveDbId, const NodeSet &blacklist = {});

        void setQueryParameters(Poco::URI &uri, bool &) final;
        virtual void setSpecificQueryParameters(Poco::URI &uri) = 0;
        virtual bool handleError(std::istream &is, const Poco::URI &uri) override;

    private:
        NodeSet _blacklist;
};

} // namespace KDC
