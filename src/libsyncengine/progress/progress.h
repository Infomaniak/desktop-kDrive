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

#include "estimates.h"

#include <cstdint>

namespace KDC {

class Progress {
    public:
        Progress();

        void update();
        Estimates estimates() const;
        inline int64_t completed() const { return _completed; }
        void setCompleted(int64_t completed);
        inline double progressPerSec() const { return _progressPerSec; }
        inline int64_t total() const { return _total; }
        inline void setTotal(int64_t value) { _total = value; }
        inline int64_t remaining() const { return (_total - _completed); }

    private:
        double _progressPerSec;
        int64_t _prevCompleted;
        double _initialSmoothing;
        int64_t _completed;
        int64_t _total;
};

} // namespace KDC
