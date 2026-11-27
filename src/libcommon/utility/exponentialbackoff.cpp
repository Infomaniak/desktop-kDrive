/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "exponentialbackoff.h"

#include <algorithm>
#include <thread>

namespace KDC {

ExponentialBackoff::ExponentialBackoff(const Duration initialBackoff, const Duration maxBackoff, const unsigned int multiplier) :
	_initialBackoff(initialBackoff),
	_maxBackoff(std::max(initialBackoff, maxBackoff)),
	_multiplier(std::max(1u, multiplier)) {}

void ExponentialBackoff::reset() {
	const std::scoped_lock lock(_mutex);
	_backoff = Duration{0};
	_start = Clock::now();
}

ExponentialBackoff::Duration ExponentialBackoff::increase() {
	const std::scoped_lock lock(_mutex);
	if (_backoff == Duration{0}) {
		_backoff = _initialBackoff;
	} else if (_backoff >= _maxBackoff / _multiplier) {
		_backoff = _maxBackoff;
	} else {
		_backoff *= _multiplier;
	}
	_start = Clock::now();
	return _backoff;
}

bool ExponentialBackoff::canProceed() const {
	return remainingTime() == Duration{0};
}

bool ExponentialBackoff::waitUntilCanProceed(const std::optional<Duration> timeout) const {
	const auto waitStart = Clock::now();
	while (true) {
		const auto remaining = remainingTime();
		if (remaining == Duration{0}) return true;

		auto sleepDuration = remaining;
		if (timeout) {
			const auto elapsed = std::chrono::duration_cast<Duration>(Clock::now() - waitStart);
			if (elapsed >= *timeout) return false;
			sleepDuration = std::min(sleepDuration, *timeout - elapsed);
		}
		std::this_thread::sleep_for(sleepDuration);
	}
}

ExponentialBackoff::Duration ExponentialBackoff::backoff() const {
	const std::scoped_lock lock(_mutex);
	return _backoff;
}

ExponentialBackoff::Duration ExponentialBackoff::remainingTime() const {
	const std::scoped_lock lock(_mutex);
	return remainingTimeNoLock();
}

ExponentialBackoff::Duration ExponentialBackoff::remainingTimeNoLock() const {
	const auto elapsed = std::chrono::duration_cast<Duration>(Clock::now() - _start);
	return elapsed >= _backoff ? Duration{0} : _backoff - elapsed;
}

} // namespace KDC
