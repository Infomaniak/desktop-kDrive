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

#include "jobs/network/abstractnetworkjob.h"
#include "libcommon/keychainmanager/apitoken.h"

namespace KDC {

class AbstractLoginJob : public AbstractNetworkJob {
    public:
        AbstractLoginJob();

        inline const ApiToken &apiToken() const { return _apiToken; }

        bool hasErrorApi(std::string *errorCode = nullptr, std::string *errorDescr = nullptr);

    protected:
        ApiToken _apiToken;

    private:
        virtual std::string getSpecificUrl() override;
        virtual std::string getUrl() override;
        virtual std::string getContentType() override;

        virtual bool handleResponse(std::istream &inputStream) override;
        virtual bool handleError(const std::string &replyBody, const Poco::URI &uri) override;

        std::string _errorCode;
        std::string _errorDescr;
};

} // namespace KDC
