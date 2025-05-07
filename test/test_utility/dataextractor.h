// Infomaniak kDrive - Desktop
// Copyright (C) 2023-2025 Infomaniak Network SA
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once
#include "utility/types.h"

namespace KDC {

class DataExtractor {
    public:
        DataExtractor();
        explicit DataExtractor(const SyncPath &absolutePath);

        void addColumHeader(const std::string &columnName);
        void addRow(const std::string &rowName = {});
        void push(const std::string &data);

        void print();

        void setAbsolutePath(const SyncPath &absolutePath) { _absolutePath = absolutePath; }

    private:
        SyncPath _absolutePath;

        std::vector<std::string> _columnHeaders;
        std::vector<std::string> _rowHeaders;
        std::vector<std::vector<std::string>> _data;
};

} // namespace KDC
