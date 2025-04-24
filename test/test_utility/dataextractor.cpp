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

#include "dataextractor.h"

#include "libcommon/utility/utility.h"

namespace KDC {

DataExtractor::DataExtractor() : DataExtractor("") {}

DataExtractor::DataExtractor(const SyncPath& absolutePath) : _absolutePath(absolutePath) {
    _data.reserve(20);
}

void DataExtractor::addColumHeader(const std::string& columnName) {
    _columnHeaders.push_back(columnName);
}

void DataExtractor::addRow(const std::string& rowName /*= {}*/) {
    _rowHeaders.push_back(rowName);
    (void) _data.emplace_back();
    _data[_data.size() - 1].reserve(20);
}

void DataExtractor::push(const std::string& data) {
    (void) _data[_data.size() - 1].emplace_back(data);
}

void DataExtractor::print() {
    if (_absolutePath.empty()) {
        const auto homePath = CommonUtility::envVarValue("HOME");
        _absolutePath = SyncPath(homePath) / "kDrive_data.txt";
    }

    std::ofstream file(_absolutePath);

    for (const auto& columnName: _columnHeaders) {
        file << ";" << columnName;
    }

    for (uint64_t i = 0; i < _data.size(); i++) {
        file << _rowHeaders[i];

        for (uint64_t j = 0; j < _data[i].size(); j++) {
            file << ";" << _data[i][j];
        }
        file << std::endl;
    }

    file.close();
}

} // namespace KDC
