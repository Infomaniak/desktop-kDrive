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

#include "testapitoken.h"
#include "keychainmanager/apitoken.h"

namespace KDC {

void TestApiToken::setUp() {}

void TestApiToken::tearDown() {}

void TestApiToken::testReconstructJson() {
    static const std::string testStr =
            "{\"refresh_token\":\"qwertzuiopasdfghjklyxcvbnm1234567890\",\"token_type\":\"Bearer\",\"expires_in\":7200,\"user_"
            "id\":"
            "\"112233\",\"scope\":\"user_email user_info drive\",\"access_token\":\"1234567890qwertzuiopasdfghjklyxcvbnm\"}";
    ApiToken apiToken(testStr);
    ApiToken apiToken2(apiToken.reconstructJsonString());
    CPPUNIT_ASSERT(apiToken == apiToken2);
}

} // namespace KDC
