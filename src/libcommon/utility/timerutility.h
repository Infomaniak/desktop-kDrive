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

#pragma once

namespace KDC {

class TimerUtility {
    public:
        static UniqueId startTimer();
        static SecondsDuration elapsed(UniqueId timerId, const std::string_view consoleMsg = {});
        static SecondsDuration lap(UniqueId timerId, const std::string_view consoleMsg = {});
        static SecondsDuration stopTimer(UniqueId timerId, const std::string_view consoleMsg = {});

    private:
        static std::unordered_map<UniqueId, std::chrono::time_point<std::chrono::steady_clock>> _timers;
        static UniqueId _timerId;
        static std::mutex _mutex;
};

} // namespace KDC
