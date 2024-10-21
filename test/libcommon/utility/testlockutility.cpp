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

#include "testlockutility.h"
#include "libcommon/utility/lockutility.h"
#include "utility/utility.h"

#include <thread>

namespace KDC {

void TestLockUtility::testLock() {
    LockUtility lock;
    CPPUNIT_ASSERT(lock.lock());
    CPPUNIT_ASSERT(!lock.lock());
    lock.unlock();
    CPPUNIT_ASSERT(lock.lock());
}

int randomSleepTime() {
    return std::rand() / ((RAND_MAX + 1u) / 10) + 1; // Generates random value between 1 and 10
}

constexpr unsigned int nbThread = 3;
constexpr unsigned int nbTest = 5;
static unsigned int alreadyLockedCounter = 0;
static unsigned int lockAcquiredCounter = 0;
static LockUtility lock;
void testCallback() {
    int i = 0;
    while (i < nbTest) {
        Utility::msleep(randomSleepTime());
        if (!lock.lock()) {
            alreadyLockedCounter++;
            Utility::msleep(randomSleepTime());
            continue;
        }
        lockAcquiredCounter++;
        Utility::msleep(10);
        lock.unlock();
        i++;
    }
}

void TestLockUtility::testLockMultiThread() {
    std::thread threadArray[nbThread];
    for (auto &t: threadArray) {
        std::thread tmp(&testCallback);
        t = std::move(tmp);
    }
    for (auto &t: threadArray) {
        t.join();
    }
    CPPUNIT_ASSERT(alreadyLockedCounter > 0);
    CPPUNIT_ASSERT_EQUAL(nbTest * nbThread, lockAcquiredCounter);
}

} // namespace KDC
