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

#include "timeouthelper.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {
bool TimeoutHelper::waitFor(std::function<bool()> condition, const std::chrono::steady_clock::duration& duration,
                            const std::chrono::steady_clock::duration& loopWait) {
    TimeoutHelper timeout(duration, loopWait);
    while (!condition()) {
        if (timeout.timedOut()) return false;
    }
    return true;
}


bool TimeoutHelper::waitFor(std::function<bool()> condition, std::function<void()> stateCheck,
                            const std::chrono::steady_clock::duration& duration,
                            const std::chrono::steady_clock::duration& loopWait) {
    TimeoutHelper timeout(duration, loopWait);
    while (!condition()) {
        stateCheck();
        if (timeout.timedOut()) return false;
    }
    return true;
}

bool TimeoutHelper::timedOut() {
    bool result = (_start + _duration) < std::chrono::steady_clock::now();
    if (!result && _loopWait != std::chrono::milliseconds(0)) {
        Utility::msleep(static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(_loopWait).count()));
    }
    return result;
}
} // namespace KDC
