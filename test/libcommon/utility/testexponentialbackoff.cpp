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

#include "testexponentialbackoff.h"

#include "libcommon/utility/exponentialbackoff.h"

#include <thread>

using namespace std::chrono_literals;

namespace KDC {

void TestExponentialBackoff::testInitialState() {
	const ExponentialBackoff backoff(100ms, 1s);
	CPPUNIT_ASSERT(backoff.canProceed());
	CPPUNIT_ASSERT(backoff.waitUntilCanProceed(0ms));
	CPPUNIT_ASSERT_EQUAL(int64_t{0}, static_cast<int64_t>(backoff.backoff().count()));
}

void TestExponentialBackoff::testIncrease() {
	ExponentialBackoff backoff(1s, 5s, 2);
	CPPUNIT_ASSERT_EQUAL(int64_t{1000}, static_cast<int64_t>(backoff.increase().count()));
	CPPUNIT_ASSERT(!backoff.canProceed());
	CPPUNIT_ASSERT_EQUAL(int64_t{2000}, static_cast<int64_t>(backoff.increase().count()));
	CPPUNIT_ASSERT_EQUAL(int64_t{4000}, static_cast<int64_t>(backoff.increase().count()));
	// Capped to max backoff
	CPPUNIT_ASSERT_EQUAL(int64_t{5000}, static_cast<int64_t>(backoff.increase().count()));
	CPPUNIT_ASSERT_EQUAL(int64_t{5000}, static_cast<int64_t>(backoff.increase().count()));
}

void TestExponentialBackoff::testReset() {
	ExponentialBackoff backoff(1s, 5s);
	(void) backoff.increase();
	(void) backoff.increase();
	CPPUNIT_ASSERT(!backoff.canProceed());

	backoff.reset();
	CPPUNIT_ASSERT(backoff.canProceed());
	CPPUNIT_ASSERT_EQUAL(int64_t{0}, static_cast<int64_t>(backoff.remainingTime().count()));
	// Restart from the initial backoff
	CPPUNIT_ASSERT_EQUAL(int64_t{1000}, static_cast<int64_t>(backoff.increase().count()));
}

void TestExponentialBackoff::testCanProceedAfterBackoff() {
	ExponentialBackoff backoff(50ms, 1s);
	(void) backoff.increase();
	CPPUNIT_ASSERT(!backoff.canProceed());
	std::this_thread::sleep_for(60ms);
	CPPUNIT_ASSERT(backoff.canProceed());
}

void TestExponentialBackoff::testWaitUntilCanProceed() {
	ExponentialBackoff backoff(50ms, 1s);
    const auto start = std::chrono::steady_clock::now();
	(void) backoff.increase();
	CPPUNIT_ASSERT(backoff.waitUntilCanProceed());
	CPPUNIT_ASSERT(std::chrono::steady_clock::now() - start >= 50ms);
	CPPUNIT_ASSERT(backoff.canProceed());
}

void TestExponentialBackoff::testWaitUntilCanProceedTimeout() {
	ExponentialBackoff backoff(10s, 20s);
	(void) backoff.increase();
	const auto start = std::chrono::steady_clock::now();
	CPPUNIT_ASSERT(!backoff.waitUntilCanProceed(50ms));
	const auto elapsed = std::chrono::steady_clock::now() - start;
	CPPUNIT_ASSERT(elapsed >= 50ms);
	CPPUNIT_ASSERT(elapsed < 5s);
	CPPUNIT_ASSERT(!backoff.canProceed());
}

} // namespace KDC
