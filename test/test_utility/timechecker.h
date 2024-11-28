/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#pragma once

class TimeChecker {
    public:
        explicit TimeChecker(bool start = false) {
            if (start) this->start();
        }
        void start() { _time = std::chrono::steady_clock::now(); }
        void stop() {
            auto end = std::chrono::steady_clock::now();
            _diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - _time).count();
        }
        bool lessOrEqualThan(long long value) {
            if (_diff > value) std::cout << "TimeChecker::lessThan: " << _diff << " >= " << value << std::endl;
            return _diff <= value;
        }
        bool greaterOrEqualThan(long long value) {
            if (_diff < value) std::cout << "TimeChecker::greaterThan: " << _diff << " <= " << value << std::endl;
            return _diff >= value;
        }
        bool between(long long min, long long max) {
            if (_diff < min || _diff > max)
                std::cout << "TimeChecker::between: " << _diff << " <= " << min << " || " << _diff << " >= " << max
                          << std::endl;
            return _diff >= min && _diff <= max;
        }

    private:
        std::chrono::steady_clock::time_point _time;
        long long _diff{0};
};
