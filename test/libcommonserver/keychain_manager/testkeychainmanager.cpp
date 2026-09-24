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
#include <stdexcept>
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
        bool readPassword(const std::string &keychainKey, std::string &data, bool &found) override {
            if (keychainKey == throwingKey) {
                throw std::runtime_error("Simulated keychain failure");
            }
            Utility::msleep(90000); // Simulate a timeout by sleeping for 90 seconds
            data = "dummy_data";
            found = true;
            return true;
        }
        bool deletePassword([[maybe_unused]] const std::string &keychainKey) override { return true; }

        bool isTesting() override { return true; }

        inline static const std::string throwingKey = "throwing_key";
};

// Storage whose read is slower than the keychain read timeout, to keep a worker in flight after the
// caller has timed out.
class MockKeyChainStorageWithSlowRead : public IKeyChainStorage {
    public:
        bool writePassword([[maybe_unused]] const std::string &keychainKey,
                           [[maybe_unused]] const std::string &rawData) override {
            return true;
        }
        bool readPassword([[maybe_unused]] const std::string &keychainKey, std::string &data, bool &found) override {
            Utility::msleep(70000); // Longer than the 60 seconds keychain read timeout
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

void TestKeychainManager::testReadPasswordThrows() {
    std::string data;
    bool found = false;
    (void) KeyChainManager::instance(std::make_shared<MockKeyChainStorageWithTimeout>());
    const auto exitInfo = KeyChainManager::instance()->readData(MockKeyChainStorageWithTimeout::throwingKey, data, found);
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::KeychainAccessError), exitInfo);
    CPPUNIT_ASSERT(!found);
}

void TestKeychainManager::testConcurrentReadLimit() {
    const auto storage = std::make_shared<MockKeyChainStorageWithTimeout>();
    (void) KeyChainManager::instance(storage);

    constexpr std::size_t concurrentReads = 10;
    for (std::size_t index = 0; index < concurrentReads; ++index) {
        std::thread([&]() {
            std::string data;
            bool found = false;
            (void) KeyChainManager::instance()->readData("dummy_key", data, found);
        }).detach();
    }

    Utility::msleep(100); // Give some time for the threads to start

    std::string data;
    bool found = false;
    const auto exitInfo = KeyChainManager::instance()->readData("dummy_key", data, found);
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::KeychainAccessError), exitInfo);
    CPPUNIT_ASSERT(!found);
}

void TestKeychainManager::testWorkerOutlivesManager() {
    if (!testhelpers::isExtendedTest()) return;

    // Install a dedicated instance whose read is slower than the timeout.
    KeyChainManager::_instance.reset();
    (void) KeyChainManager::instance(std::make_shared<MockKeyChainStorageWithSlowRead>());

    // Keep a reference on the in-flight counter to check that the worker releases its slot even once
    // the manager is gone.
    const auto inFlightReads = KeyChainManager::_instance->_inFlightReadThreads;

    std::thread reader([]() {
        std::string data;
        bool found = false;
        (void) KeyChainManager::instance()->readData("dummy_key", data, found);
    });
    reader.join(); // Returns after the 60 seconds timeout, while the worker is still blocked

    // Destroy the singleton while the worker is still running: the worker must not dereference the
    // destroyed manager, and must still release its in-flight slot.
    KeyChainManager::_instance.reset();

    for (int attempt = 0; attempt < 300 && inFlightReads->load() != 0; ++attempt) {
        Utility::msleep(100);
    }
    CPPUNIT_ASSERT_EQUAL(static_cast<uint16_t>(0), inFlightReads->load());

    // Restore a valid instance for the remaining tests.
    (void) KeyChainManager::instance(std::make_shared<MockKeyChainStorageWithTimeout>());
}

} // namespace KDC
