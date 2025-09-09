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

#include "abstractcommchannel.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

AbstractCommChannel::~AbstractCommChannel() {}

bool AbstractCommChannel::open() {
    return true;
}

std::string AbstractCommChannel::id() {
    return std::to_string(reinterpret_cast<uintptr_t>(this));
}

CommString AbstractCommChannel::truncateLongLogMessage(const CommString &message) {
    if (static const size_t maxLogMessageSize = 2048; message.size() > maxLogMessageSize) {
        return message.substr(0, maxLogMessageSize) + Str(" (truncated)");
    }

    return message;
}

} // namespace KDC
