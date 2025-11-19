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

namespace KDC {

/**
 * @brief Utility class to extract and store error information returned by the infomaniak API.
 * None of the field are mandatory!
 * The expected JSON object must be in the form:
 * {"code":"object_not_found","description":"Object not found","context":{"model":"Drive"}}
 * or:
 * {"result":"error","error":{"code":"object_not_found","description":"Object not found","context":{"model":"Drive"}}}
 */
class BackError {
    public:
        explicit BackError(const Poco::JSON::Object::Ptr jsonObjPtr);

        [[nodiscard]] const std::string &code() const { return _code; }
        [[nodiscard]] const std::string &description() const { return _description; }
        [[nodiscard]] const std::string &contextReason() const { return _contextReason; }
        [[nodiscard]] const std::string &contextModel() const { return _contextModel; }

    private:
        void extractFromFullReply(const Poco::JSON::Object::Ptr jsonObjPtr);
        void extractFromErrorObject(const Poco::JSON::Object::Ptr jsonObjPtr);

        std::string _code;
        std::string _description;
        std::string _contextReason;
        std::string _contextModel;
};

} // namespace KDC
