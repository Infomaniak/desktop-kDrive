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

#pragma once

#include <chrono>
#include <mutex>
#include <optional>

namespace KDC {

class ExponentialBackoff {
	public:
		using Clock = std::chrono::steady_clock;
		using Duration = std::chrono::milliseconds;

		ExponentialBackoff(Duration initialBackoff, Duration maxBackoff, double multiplier = 2);

		// Set the backoff to 0, the caller can proceed immediately.
		void reset();
		// Increase the backoff, restart the internal timer and return the remaining time.
		Duration increase();
		// Return true if the internal timer has reached the current backoff.
		bool canProceed() const;
		// Block until the caller can proceed. Return false if `timeout` is reached first.
		bool waitUntilCanProceed(std::optional<Duration> timeout = std::nullopt) const;

		Duration backoff() const;
		Duration remainingTime() const;

	private:
		Duration remainingTimeNoLock() const;

		const Duration _initialBackoff;
		const Duration _maxBackoff;
        const double _multiplier;

		mutable std::mutex _mutex;
		Duration _backoff{0};
		Clock::time_point _start{Clock::now()};
};

} // namespace KDC
