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

// Dedicated namespace so that Q_NAMESPACE does not cover the whole KDC namespace: QML reads these values as
// `SyncControl.Pause`, ... (registered in AppClientLinux::setupQmlEngine).
namespace SyncControl {
Q_NAMESPACE

// Pause or resume control of one synchronization.
enum class SyncControlState : uint8_t {
    Disabled = 0,
    Pending,
    Pause,
    Resume,
};
Q_ENUM_NS(SyncControlState)

} // namespace SyncControl

using SyncControl::SyncControlState;

} // namespace KDC
