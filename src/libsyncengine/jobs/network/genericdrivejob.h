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

#include "abstractnetworkjob.h"

namespace KDC {

class AbstractTokenNetworkJob : public AbstractNetworkJob {
    public:
        AbstractTokenNetworkJob(int driveId);

        bool hasErrorApi(std::string *errorCode = nullptr, std::string *errorDescr = nullptr);

    protected:
        virtual std::string getSpecificUrl() override;

    private:
        virtual void addRawHeader(HTTPRequest &req) override;
        virtual void parseError(std::istream &is, Poco::URI uri) override;
        virtual std::string getUrl() override;

        std::string _baseApiUrl;
        std::string _token;
        std::string _errorCode;
        std::string _errorDescr;
        int _driveId;
        Poco::JSON::Object::Ptr _error = nullptr;
};

}  // namespace KDC
