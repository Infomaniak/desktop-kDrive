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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "app/mainwindow/upselloffer.h"

#include "libcommon/data/drive.h"
#include "libcommon/utility/types.h"

namespace KDC::UpsellOffers {

namespace {

Offer offerForTarget(const UpsellTarget target) {
    switch (target) {
        case UpsellTarget::StandAloneToMykSuitePlus:
        case UpsellTarget::MykSuiteToMykSuitePlus:
            return Offer::MyKSuitePlus;
        case UpsellTarget::StandAloneTokSuitePro:
        case UpsellTarget::kSuiteUpgrade:
            return Offer::KSuitePro;
        case UpsellTarget::Unknown:
        case UpsellTarget::EnumEnd:
            break;
    }
    return Offer::None;
}

} // namespace

Offer offerFor(const Drive &drive) {
    if (!drive.admin() || !drive.packInfo().isFree()) {
        return Offer::None;
    }
    return offerForTarget(drive.packInfo().upsellTarget());
}

} // namespace KDC::UpsellOffers
