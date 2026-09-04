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

#include "testkeychainmanager.h"

#include "keychainmanager/keychainmanager.h"
#include "keychainmanager/keychainstorage.h"
#include "test_utility/testhelpers.h"
#include "utility/timerutility.h"
#include "utility/utility.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace KDC {

namespace {
class MockKeyChainStorageWithTimeout : public IKeyChainStorage {
    public:
        bool writePassword([[maybe_unused]] const std::string &keychainKey,
                           [[maybe_unused]] const std::string &rawData) override {
            return true;
        }
        bool readPassword([[maybe_unused]] const std::string &keychainKey, std::string &data, bool &found) override {
            Utility::msleep(90000); // Simulate a timeout by sleeping for 90 seconds
            data = "dummy_data";
            found = true;
            return true;
        }
        bool deletePassword([[maybe_unused]] const std::string &keychainKey) override { return true; }

        bool isTesting() override { return true; }
};
} // namespace

void TestKeychainManager::testTimeOut() {
    if (!testhelpers::isExtendedTest()) return;

    const TimerUtility timer;

    std::string data;
    bool found = false;
    (void) KeyChainManager::instance(std::make_shared<MockKeyChainStorageWithTimeout>());
    const auto exitInfo = KeyChainManager::instance()->readData("dummy_key", data, found);
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::KeychainAccessTimeout), exitInfo);
    // Ensure that the timeout occurred after 60 seconds and before 90 seconds
    CPPUNIT_ASSERT_GREATEREQUAL(std::chrono::seconds(60).count(), timer.elapsed<std::chrono::seconds>().count());
    CPPUNIT_ASSERT_LESS(std::chrono::seconds(90).count(), timer.elapsed<std::chrono::seconds>().count());
}

void TestKeychainManager::testConcurrentReadLimit() {
    const auto storage = std::make_shared<MockKeyChainStorageWithTimeout>();
    (void) KeyChainManager::instance(storage);

    constexpr std::size_t concurrentReads = 10;
    for (std::size_t index = 0; index < concurrentReads; ++index) {
        std::thread([&]() {
            std::string data;
            bool found = false;
            const auto exitInfo = KeyChainManager::instance()->readData("dummy_key", data, found);
        }).detach();
    }

    Utility::msleep(100); // Give some time for the threads to start

    std::string data;
    bool found = false;
    const auto exitInfo = KeyChainManager::instance()->readData("dummy_key", data, found);
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::KeychainAccessError), exitInfo);
    CPPUNIT_ASSERT(!found);
}

} // namespace KDC
