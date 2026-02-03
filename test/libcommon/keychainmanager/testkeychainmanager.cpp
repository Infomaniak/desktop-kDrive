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

#include "testkeychainmanager.h"
#include "libcommon/keychainmanager/keychainmanager.h"

namespace KDC {

void TestKeyChainManager::testWriteAndReadToken() {
    auto instance = KeyChainManager::instance(true); // Testing mode
    CPPUNIT_ASSERT_MESSAGE("KeyChainManager instance should not be null", instance != nullptr);

    std::string testKey = "test_key_1";
    std::string testData = "test_data_123";
    bool found = false;
    ApiToken readToken;

    // Write token
    bool writeResult = instance->writeToken(testKey, testData);
    CPPUNIT_ASSERT_MESSAGE("Write token should succeed", writeResult);

    // Read token back
    bool readResult = instance->readApiToken(testKey, readToken, found);
    CPPUNIT_ASSERT_MESSAGE("Read token should succeed", readResult);
    CPPUNIT_ASSERT_MESSAGE("Token should be found", found);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Token data should match", std::string(readToken.refreshToken()), testData);
}

void TestKeyChainManager::testDeleteToken() {
    auto instance = KeyChainManager::instance(true);

    std::string testKey = "test_delete_key";
    std::string testData = "delete_me";
    bool found = false;
    ApiToken readToken;

    // Write then delete
    instance->writeToken(testKey, testData);
    bool deleteResult = instance->deleteToken(testKey);
    CPPUNIT_ASSERT_MESSAGE("Delete should succeed", deleteResult);

    // Verify deleted
    instance->readApiToken(testKey, readToken, found);
    CPPUNIT_ASSERT_MESSAGE("Token should not be found after deletion", !found);
}

void TestKeyChainManager::testReadTokenNotFound() {
    auto instance = KeyChainManager::instance(true);

    std::string nonExistentKey = "non_existent_key_xyz";
    bool found = false;
    ApiToken readToken;

    bool readResult = instance->readApiToken(nonExistentKey, readToken, found);
    CPPUNIT_ASSERT_MESSAGE("Read should succeed even for missing key", readResult);
    CPPUNIT_ASSERT_MESSAGE("Token should not be found", !found);
}

void TestKeyChainManager::testReadWriteData() {
    auto instance = KeyChainManager::instance(true);

    std::string testKey = "test_data_key";
    std::string testData = "raw_data_content";
    std::string readData;
    bool found = false;

    // Write data
    bool writeResult = instance->writeToken(testKey, testData);
    CPPUNIT_ASSERT_MESSAGE("Write should succeed", writeResult);

    // Read data back
    bool readResult = instance->readDataFromKeystore(testKey, readData, found);
    CPPUNIT_ASSERT_MESSAGE("Read data should succeed", readResult);
    CPPUNIT_ASSERT_MESSAGE("Data should be found", found);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Data should match", testData, readData);
}

void TestKeyChainManager::testSingletonPattern() {
    // Get first instance
    auto instance1 = KeyChainManager::instance(true);
    CPPUNIT_ASSERT_MESSAGE("First instance should not be null", instance1 != nullptr);

    // Get second instance
    auto instance2 = KeyChainManager::instance(true);
    CPPUNIT_ASSERT_MESSAGE("Second instance should not be null", instance2 != nullptr);

    // Should be the same instance
    CPPUNIT_ASSERT_MESSAGE("Instances should be identical", instance1.get() == instance2.get());
}

void TestKeyChainManager::testWriteDummyTest() {
    auto instance = KeyChainManager::instance(true);

    // In testing mode, this should succeed
    bool result = instance->writeDummyTest();
    CPPUNIT_ASSERT_MESSAGE("Write dummy test should succeed in testing mode", result);

    // Clean up
    instance->clearDummyTest();
}

} // namespace KDC
