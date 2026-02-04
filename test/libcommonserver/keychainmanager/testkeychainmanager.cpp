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
#include "mocks/libcommonserver/keychainmanager/mockkeychainstore.h"

#include "libcommonserver/keychainmanager/keychainmanager.h"
#include "libcommonserver/keychainmanager/keychainstore.h"


namespace KDC {

void TestKeyChainManager::testWriteAndReadToken() {
    auto mockStore = std::make_unique<MockKeychainStore>();
    auto *mockPtr = mockStore.get();
    KeyChainManager manager(std::move(mockStore));

    // Generate test token data string
    ApiToken apiToken;
    apiToken.setAccessToken("access_token:1");
    apiToken.setRefreshToken("refresh_token:2");
    apiToken.setTokenType("special_token_type");
    apiToken.setExpiresIn(3600);
    apiToken.setUserId(1);
    apiToken.setScope("token_scope");

    std::string testData = apiToken.reconstructJsonString();

    bool found = false;
    std::string testKey = "test_key_1";

    // Write token
    const bool writeResult = manager.writeToken(testKey, testData);
    CPPUNIT_ASSERT_MESSAGE("Write token should succeed", writeResult);

    // Verify mock has the data
    CPPUNIT_ASSERT_MESSAGE("Mock store should contain the key",
                           mockPtr->hasKey(KeyChainManager::DEFAULT_PACKAGE, KeyChainManager::DEFAULT_SERVICE, testKey));

    // Read token back
    ApiToken readToken;
    const bool readResult = manager.readApiToken(testKey, readToken, found);
    CPPUNIT_ASSERT_MESSAGE("Read token should succeed", readResult);
    CPPUNIT_ASSERT_MESSAGE("Token should be found", found);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Token data should match", testData, readToken.reconstructJsonString());
}

void TestKeyChainManager::testDeleteToken() {
    auto mockStore = std::make_unique<MockKeychainStore>();
    auto *mockPtr = mockStore.get();
    KeyChainManager manager(std::move(mockStore));

    const std::string testKey = "test_delete_key";
    const std::string testData = "delete_me";

    // Write then delete
    manager.writeToken(testKey, testData);
    CPPUNIT_ASSERT_MESSAGE("Key should exist before deletion",
                           mockPtr->hasKey(KeyChainManager::DEFAULT_PACKAGE, KeyChainManager::DEFAULT_SERVICE, testKey));

    const bool deleteResult = manager.deleteToken(testKey);
    CPPUNIT_ASSERT_MESSAGE("Delete should succeed", deleteResult);

    // Verify deleted via mock
    CPPUNIT_ASSERT_MESSAGE("Key should not exist after deletion",
                           !mockPtr->hasKey(KeyChainManager::DEFAULT_PACKAGE, KeyChainManager::DEFAULT_SERVICE, testKey));

    // Verify via manager
    bool found = false;
    ApiToken readToken;
    CPPUNIT_ASSERT(manager.readApiToken(testKey, readToken, found));
    CPPUNIT_ASSERT_MESSAGE("Token should not be found after deletion", !found);
}

void TestKeyChainManager::testReadTokenNotFound() {
    auto mockStore = std::make_unique<MockKeychainStore>();
    KeyChainManager manager(std::move(mockStore));

    const std::string nonExistentKey = "non_existent_key_xyz";
    bool found = false;
    ApiToken readToken;

    const bool readResult = manager.readApiToken(nonExistentKey, readToken, found);
    CPPUNIT_ASSERT_MESSAGE("Read should succeed even for missing key", readResult);
    CPPUNIT_ASSERT_MESSAGE("Token should not be found", !found);
}

void TestKeyChainManager::testReadWriteData() {
    auto mockStore = std::make_unique<MockKeychainStore>();
    KeyChainManager manager(std::move(mockStore));

    std::string testKey = "test_data_key";
    std::string testData = "raw_data_content";

    // Write data
    const bool writeResult = manager.writeToken(testKey, testData);
    CPPUNIT_ASSERT_MESSAGE("Write should succeed", writeResult);

    // Read data back using readDataFromKeystore
    std::string readData;
    bool found = false;
    const bool readResult = manager.readDataFromKeystore(testKey, readData, found);
    CPPUNIT_ASSERT_MESSAGE("Read data should succeed", readResult);
    CPPUNIT_ASSERT_MESSAGE("Data should be found", found);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Data should match", testData, readData);
}

void TestKeyChainManager::testWriteDummyTest() {
    auto mockStore = std::make_unique<MockKeychainStore>();
    auto *mockPtr = mockStore.get();
    KeyChainManager manager(std::move(mockStore));

    // Should succeed with mock
    const bool result = manager.writeDummyTest();
    CPPUNIT_ASSERT_MESSAGE("Write dummy test should succeed with mock", result);

    // Verify the dummy key was written
    CPPUNIT_ASSERT_MESSAGE("Dummy key should be in mock store",
                           mockPtr->hasKey(KeyChainManager::DEFAULT_PACKAGE, KeyChainManager::DEFAULT_SERVICE,
                                           KeyChainManager::dummyKeychainKey));

    // Clean up
    manager.clearDummyTest();
    CPPUNIT_ASSERT_MESSAGE("Dummy key should be removed after clear",
                           !mockPtr->hasKey(KeyChainManager::DEFAULT_PACKAGE, KeyChainManager::DEFAULT_SERVICE,
                                            KeyChainManager::dummyKeychainKey));
}

void TestKeyChainManager::testNullStoreThrows() {
    // Test that passing nullptr throws an exception
    try {
        KeyChainManager manager(nullptr);
        CPPUNIT_FAIL("KeyChainManager constructor should throw for null store");
    } catch (const std::invalid_argument &e) {
        // Expected
        CPPUNIT_ASSERT_MESSAGE("Exception message should mention null", std::string(e.what()).find("null") != std::string::npos);
    }
}

} // namespace KDC
