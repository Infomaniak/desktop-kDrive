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
#include "../../../update_detection/file_system_observer/snapshot/snapshotitem.h"

namespace KDC {

class JsonFullFileListWithCursorJob : public AbstractTokenNetworkJob {
    public:
        JsonFullFileListWithCursorJob(int driveDbId, const NodeId &dirId, std::list<NodeId> blacklist = {}, bool zip = true);

    private:
        virtual std::string getSpecificUrl() override;
        virtual void setQueryParameters(Poco::URI &uri, bool &canceled) override;
        inline virtual void setData(bool &canceled) override { canceled = false; }

        virtual bool handleResponse(std::istream &is) override;

        NodeId _dirId;
        std::list<NodeId> _blacklist;
        bool _zip = true;
};

}  // namespace KDC
