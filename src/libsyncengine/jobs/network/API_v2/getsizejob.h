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

#include "abstracttokennetworkjob.h"

namespace KDC {

class GetSizeJob : public AbstractTokenNetworkJob {
    public:
        GetSizeJob(int userDbId, int driveId, const NodeId &nodeId);
        GetSizeJob(int driveDbId, const NodeId &nodeId);

        inline const NodeId &nodeId() const { return _nodeId; }
        inline int64_t size() const { return _size; }

    protected:
        virtual bool handleResponse(std::istream &is) override;
        virtual bool handleError(std::istream &is, const Poco::URI &uri) override;

    private:
        virtual std::string getSpecificUrl() override;
        virtual void setQueryParameters(Poco::URI &, bool &canceled) override { canceled = false; }
        virtual void setData(bool &canceled) override { canceled = false; }

        NodeId _nodeId;
        int64_t _size;
};
} // namespace KDC
