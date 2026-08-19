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

#include "libcommon/data/exclusionapp.h"
#include "libcommon/utility/utility.h"

static const auto appIdKey = "appId";
static const auto descriptionKey = "description";
static const auto defKey = "def";

namespace KDC {

ExclusionApp::ExclusionApp(const std::string &appId, const std::string &description, bool def) :
    _appId(appId),
    _description(description),
    _def(def) {}

void ExclusionApp::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, appIdKey, _appId);
    CommonUtility::writeValueToStruct(dstruct, descriptionKey, _description);
    CommonUtility::writeValueToStruct(dstruct, defKey, _def);
}

void ExclusionApp::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, appIdKey, _appId);
    CommonUtility::readValueFromStruct(dstruct, descriptionKey, _description);
    CommonUtility::readValueFromStruct(dstruct, defKey, _def);
}

} // namespace KDC
