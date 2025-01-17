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

#include "libparms/parmslib.h"
#include "libcommon/utility/types.h"

#include <string>

namespace KDC {

class PARMS_EXPORT UploadSessionToken {
    public:
        UploadSessionToken();
        UploadSessionToken(const std::string &token);
        UploadSessionToken(int64_t dbId, const std::string &token);

        inline int64_t dbId() const { return _dbId; }
        inline const std::string &token() const { return _token; }

    private:
        int64_t _dbId;
        std::string _token;
};

} // namespace KDC
