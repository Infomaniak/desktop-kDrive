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

#pragma once

#include <QObject>

#include <cstdint>

namespace KDC {

class Drive;

// Dedicated namespace so that Q_NAMESPACE does not cover the whole KDC namespace: QML reads these values as
// `UpsellOffers.MyKSuitePlus`, ... (registered in AppClientLinux::setupQmlEngine).
namespace UpsellOffers {
Q_NAMESPACE

/** Subscription offered to upgrade a drive. */
enum class Offer : uint8_t {
    None,
    MyKSuitePlus,
    KSuitePro
};
Q_ENUM_NS(Offer)

/** Only the administrators of a drive with a free pack are offered an upgrade. */
[[nodiscard]] Offer offerFor(const Drive &drive);

} // namespace UpsellOffers

using UpsellOffers::Offer;

} // namespace KDC
