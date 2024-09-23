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

#include "utility/types.h"
#include "libparms/db/parameters.h"
#include "libparms/db/exclusiontemplate.h"

#include <regex>
#include <string>

#pragma once

namespace KDC {

class FileExclude {
    public:
        static std::shared_ptr<FileExclude> instance();

        bool isExcluded(const SyncPath &relativePath, bool syncHiddenFiles);

    private:
        /*
         * Make sure _regexPattern is up to date
         * Use this function to access _regexPatterns
         **/
        const std::vector<std::pair<std::regex, ExclusionTemplate>> &getRegexPatterns();

        void escapeRegexSpecialChar(std::string &in);

        static std::shared_ptr<FileExclude> _instance;

        std::vector<std::pair<std::regex, ExclusionTemplate>> _regexPatterns;
};

} // namespace KDC
