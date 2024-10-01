//
// Created by Clément on 20/06/2024.
//

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
