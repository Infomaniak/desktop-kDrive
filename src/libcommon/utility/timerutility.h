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
        /**
         * @brief Timer is started at creation.
         */
        TimerUtility() :
            _startTime(std::chrono::steady_clock::now()) {}

        /**
         * @brief Restart the timer.
         */
        void restart() { _startTime = std::chrono::steady_clock::now(); }

        /**
         * @brief Return the elapsed time since start.
         * @return Elapsed time in seconds.
         */
        template<typename T>
        T elapsed() const {
            return std::chrono::duration_cast<T>(timeDiff());
        }

        /**
         * @brief Return the elapsed time since start and restart the timer.
         * @return Elapsed time in seconds.
         */
        template<typename T>
        T lap() {
            const auto elapsedSeconds = elapsed<T>();
            restart();
            return elapsedSeconds;
        }

    private:
        DoubleSeconds timeDiff() const {
            const auto duration = std::chrono::steady_clock::now() - _startTime;
            return duration;
        }

        std::chrono::time_point<std::chrono::steady_clock> _startTime;
};

} // namespace KDC
