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

#pragma once
#include <chrono>
#include <functional>
#include <iostream>

namespace KDC {
class TimeoutHelper {
    public:
        /* Wait for condition() to return true,
         * Return false if timed out */
        static bool waitFor(std::function<bool()> condition, const std::chrono::steady_clock::duration& duration,
                            const std::chrono::steady_clock::duration& loopWait);

        /* Wait for condition() to return true, and run stateCheck() after each loop,
         * Return false if timed out */
        static bool waitFor(std::function<bool()> condition, std::function<void()> stateCheck,
                            const std::chrono::steady_clock::duration& duration,
                            const std::chrono::steady_clock::duration& loopWait);

        /* Waits for function() to return. If, when function() returns, more than the specified duration has elapsed, it will
         * return false. */
        template<typename T>
        static bool checkExecutionTime(std::function<T()> function, T& result,
                                       const std::chrono::steady_clock::duration& maximumDuration) {
            TimeoutHelper timeoutHelper(maximumDuration);
            result = function();
            return !timeoutHelper.timedOut();
        }

        /* Waits for function() to return. If, when function() returns, less than minimumDuration or more than maximumDuration has
         * elapsed, it will return false. */
        template<typename T>
        static bool checkExecutionTime(std::function<T()> function, T& result,
                                       const std::chrono::steady_clock::duration& minimumDuration,
                                       const std::chrono::steady_clock::duration& maximumDuration) {
            TimeoutHelper timeoutHelper(maximumDuration);
            result = function();
            if (timeoutHelper.elapsed() < minimumDuration) {
                std::cout << std::endl
                          << "Execution (" << timeoutHelper.elapsed().count() << "ms) time is less than minimum duration ("
                          << std::chrono::duration_cast<std::chrono::milliseconds>(minimumDuration).count() << "ms)" << std::endl;
                return false;
            }
            if (timeoutHelper.elapsed() > maximumDuration) {
                std::cout << std::endl
                          << "Execution (" << timeoutHelper.elapsed().count() << "ms) time is more than maximum duration ("
                          << std::chrono::duration_cast<std::chrono::milliseconds>(maximumDuration).count() << "ms)" << std::endl;
                return false;
            }

            return true;
        }

    private:
        TimeoutHelper(const std::chrono::steady_clock::duration& duration,
                      const std::chrono::steady_clock::duration& loopWait = std::chrono::milliseconds(0)) :
            _start(std::chrono::steady_clock::now()), _duration(duration), _loopWait(loopWait) {}

        bool timedOut();
        std::chrono::milliseconds elapsed() const {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _start);
        }
        std::chrono::steady_clock::time_point _start;
        std::chrono::steady_clock::duration _duration;
        std::chrono::steady_clock::duration _loopWait;
};
} // namespace KDC
