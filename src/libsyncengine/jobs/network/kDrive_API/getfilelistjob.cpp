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

#include "getfilelistjob.h"

namespace KDC {

GetFileListJob::GetFileListJob(const int userDbId, const int driveId, const NodeId &fileId, const uint64_t page /*= 1*/,
                               const bool dirOnly /*= false*/) :
    GetRootFileListJob(userDbId, driveId, page, dirOnly),
    _fileId(fileId) {}

GetFileListJob::GetFileListJob(const int driveDbId, const NodeId &fileId, const uint64_t page /*= 1*/,
                               const bool dirOnly /*= false*/) :
    GetRootFileListJob(driveDbId, page, dirOnly),
    _fileId(fileId) {}

std::string GetFileListJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _fileId;
    str += "/files";
    return str;
}

} // namespace KDC
