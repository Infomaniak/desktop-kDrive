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

#include "nodeversioninfo.h"
#include "utility/utility.h"

static const auto nodeVersionInfoAuthorName = "authorName";
static const auto nodeVersionInfoFileSize = "fileSize";
static const auto nodeVersionInfoLastModificationDate = "lastModificationDate";

namespace KDC {

NodeVersionInfo::NodeVersionInfo(const std::string &authorName, int64_t fileSize, SyncTime lastModificationDate) :
    _authorName(authorName),
    _fileSize(fileSize),
    _lastModificationDate(lastModificationDate) {}

void NodeVersionInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, nodeVersionInfoAuthorName, _authorName);
    CommonUtility::writeValueToStruct(dstruct, nodeVersionInfoFileSize, _fileSize);
    CommonUtility::writeValueToStruct(dstruct, nodeVersionInfoLastModificationDate, _lastModificationDate);
}

void NodeVersionInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, nodeVersionInfoAuthorName, _authorName);
    CommonUtility::readValueFromStruct(dstruct, nodeVersionInfoFileSize, _fileSize);
    CommonUtility::readValueFromStruct(dstruct, nodeVersionInfoLastModificationDate, _lastModificationDate);
}

} // namespace KDC
