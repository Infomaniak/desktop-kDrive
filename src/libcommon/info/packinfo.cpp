/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "packinfo.h"

#include "libcommon/utility/utility.h"

namespace KDC {

static const auto packId = "id";
static const auto packName = "name";
static const auto packDisplayName = "displayName";
static const auto packIsFree = "isFree";

PackInfo::PackInfo(const uint64_t id, const std::string &name, const std::string &displayName, const bool isFree) :
    _id(id),
    _name(name),
    _displayName(displayName),
    _isFree(isFree) {}

void PackInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, packId, _id);
    CommonUtility::writeValueToStruct(dstruct, packName, CommonUtility::str2CommString(_name));
    CommonUtility::writeValueToStruct(dstruct, packDisplayName, CommonUtility::str2CommString(_displayName));
    CommonUtility::writeValueToStruct(dstruct, packIsFree, _isFree);
}

void PackInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, packId, _id);
    CommString name;
    CommonUtility::readValueFromStruct(dstruct, packName, name);
    _name = CommonUtility::commString2Str(name);
    CommString displayName;
    CommonUtility::readValueFromStruct(dstruct, packDisplayName, displayName);
    _displayName = CommonUtility::commString2Str(displayName);
    CommonUtility::readValueFromStruct(dstruct, packIsFree, _isFree);
}

} // namespace KDC
