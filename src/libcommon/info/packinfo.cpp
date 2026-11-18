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
static const auto packSuiteId = "kSuiteId";
static const auto packUpsellTarget = "upsellTarget";

constexpr UpsellTarget getUpsellTarget(const uint64_t packId_) {
    switch (packId_) {
        case 1: // Solo
        case 6: // Free
        case 22: // Solo special offer
            return UpsellTarget::StandAloneToMykSuitePlus;
        case 8: // kSuite standard
        case 11: // kSuite pro
        case 14: // kSuite enterprise
        case 29: // kSuite essential
        case 23: // kSuite special offer 1
        case 26: // kSuite special offer 2
            return UpsellTarget::kSuiteUpgrade;
        case 16: // myKsuite
        case 19: // myKsuite+
            return UpsellTarget::MykSuiteToMykSuitePlus;
        case 2: // Team
        case 3: // Pro
            return UpsellTarget::StandAloneTokSuitePro;
        default:
            return UpsellTarget::Unknown;
    }
}

PackInfo::PackInfo(const uint64_t id, const std::string &name, const std::string &displayName, const bool isFree) :
    _id(id),
    _name(name),
    _displayName(displayName),
    _isFree(isFree) {}

void PackInfo::setDisplayName(const std::string_view displayName) {
    static constexpr std::string_view kDrivePrefix = "kDrive";
    static constexpr std::string_view kSuitePrefix = "kSuite";

    std::string name(displayName);

    // Replace all occurrences of "business" with "pro"
    constexpr std::string_view from = "Business";
    constexpr std::string_view to = "Pro";

    size_t pos = 0;
    while ((pos = name.find(from, pos)) != std::string::npos) {
        (void) name.replace(pos, from.size(), to);
        pos += to.size();
    }

    if (name.find(kDrivePrefix) != std::string::npos || name.find(kSuitePrefix) != std::string::npos) {
        _displayName = std::move(name);
        return;
    }

    switch (_id) {
        case 1: // Solo
        case 2: // Team
        case 3: // Pro
        case 6: // Free
        case 22: // Solo special offer
            _displayName = std::string(kDrivePrefix) + " " + name;
            break;

        case 8: // kSuite standard
        case 14: // kSuite enterprise
        case 29: // kSuite essential
        case 23: // kSuite special offer 1
        case 26: // kSuite special offer 2
            _displayName = std::string(kSuitePrefix) + " " + name;
            break;

        default:
            _displayName = std::move(name);
            break;
    }
}

UpsellTarget PackInfo::upsellTarget() const {
    return getUpsellTarget(_id);
}

void PackInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, packId, _id);
    CommonUtility::writeValueToStruct(dstruct, packName, CommonUtility::str2CommString(_name));
    CommonUtility::writeValueToStruct(dstruct, packDisplayName, CommonUtility::str2CommString(_displayName));
    CommonUtility::writeValueToStruct(dstruct, packIsFree, _isFree);
    CommonUtility::writeValueToStruct(dstruct, packUpsellTarget, upsellTarget());
    CommonUtility::writeValueToStruct(dstruct, packSuiteId, _kSuiteId);
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
    CommonUtility::readValueFromStruct(dstruct, packSuiteId, _kSuiteId);
}

} // namespace KDC
