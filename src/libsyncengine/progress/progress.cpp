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

#include "progress.h"

#include <algorithm>

namespace KDC {

Progress::Progress() : _progressPerSec(0), _prevCompleted(0), _initialSmoothing(1.0), _completed(0), _total(0) {}

void Progress::update() {
    const double smoothing = 0.9 * (1.0 - _initialSmoothing);
    _initialSmoothing *= 0.7;
    _progressPerSec = smoothing * _progressPerSec + (1.0 - smoothing) * static_cast<double>(_completed - _prevCompleted);
    _prevCompleted = _completed;
}

Estimates Progress::estimates() const {
    Estimates est;
    est.setEstimatedBandwidth(int64_t(_progressPerSec));
    if (_progressPerSec != 0.0) {
        est.setEstimatedEta(static_cast<int64_t>(static_cast<double>(_total - _completed) / _progressPerSec * 1000.0));
    } else {
        est.setEstimatedEta(0);
    }
    return est;
}

void Progress::setCompleted(int64_t completed) {
    _completed = std::min(completed, _total);
    _prevCompleted = std::min(_prevCompleted, _completed);
}

} // namespace KDC
