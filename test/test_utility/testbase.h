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
#include <iostream>
#include <cppunit/extensions/HelperMacros.h>

class TestBase {
    public:
        virtual void start(void) { _start = std::chrono::steady_clock::now(); }
        virtual void stop(void) {
            const auto duration =
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _start);
            if (duration.count() > 50) std::cout << " (" << duration.count() << " ms)";
        }

    private:
        std::chrono::steady_clock::time_point _start;
};
