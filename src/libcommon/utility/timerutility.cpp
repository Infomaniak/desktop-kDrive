// Infomaniak kDrive - Desktop
// Copyright (C) 2023-2025 Infomaniak Network SA
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "timerutility.h"

#include <iostream>

namespace KDC {

std::unordered_map<UniqueId, std::chrono::time_point<std::chrono::steady_clock>> TimerUtility::_timers;
UniqueId TimerUtility::_timerId = 0;
std::mutex TimerUtility::_mutex;

UniqueId TimerUtility::startTimer() {
    const std::scoped_lock<std::mutex> lock(_mutex);
    _timerId++;
    (void) _timers.try_emplace(_timerId, std::chrono::steady_clock::now());
    return _timerId;
}

SecondsDuration TimerUtility::elapsed(const UniqueId timerId, const std::string_view consoleMsg /*= {}*/) {
    if (!_timers.contains(timerId)) return SecondsDuration();

    const SecondsDuration elapsedSeconds = std::chrono::steady_clock::now() - _timers[timerId];
    if (!consoleMsg.empty()) {
        std::cout << consoleMsg << " : " << elapsedSeconds << std::endl;
    }
    return elapsedSeconds;
}

SecondsDuration TimerUtility::lap(const UniqueId timerId, const std::string_view consoleMsg /*= {}*/) {
    const auto elapsedSeconds = elapsed(timerId, consoleMsg);
    _timers[timerId] = std::chrono::steady_clock::now();
    return elapsedSeconds;
}

SecondsDuration TimerUtility::stopTimer(const UniqueId timerId, const std::string_view consoleMsg /*= {}*/) {
    const auto elapsedSeconds = elapsed(timerId, consoleMsg);
    (void) _timers.erase(timerId);
    return elapsedSeconds;
}

} // namespace KDC
