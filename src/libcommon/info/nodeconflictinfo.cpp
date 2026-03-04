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

#include "nodeconflictinfo.h"
#include "utility/utility.h"

static const auto nodeConflictInfoAuthorName = "authorName";
static const auto nodeConflictInfoFileSize = "fileSize";
static const auto nodeConflictInfoLastModificationDate = "lastModificationDate";

namespace KDC {

NodeConflictInfo::NodeConflictInfo(const std::string &authorName, const int64_t fileSize, const SyncTime lastModificationDate) :
    _authorName(authorName),
    _fileSize(fileSize),
    _lastModificationDate(lastModificationDate) {}

void NodeConflictInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, nodeConflictInfoAuthorName, _authorName);
    CommonUtility::writeValueToStruct(dstruct, nodeConflictInfoFileSize, _fileSize);
    CommonUtility::writeValueToStruct(dstruct, nodeConflictInfoLastModificationDate, _lastModificationDate);
}

void NodeConflictInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, nodeConflictInfoAuthorName, _authorName);
    CommonUtility::readValueFromStruct(dstruct, nodeConflictInfoFileSize, _fileSize);
    CommonUtility::readValueFromStruct(dstruct, nodeConflictInfoLastModificationDate, _lastModificationDate);
}

} // namespace KDC
