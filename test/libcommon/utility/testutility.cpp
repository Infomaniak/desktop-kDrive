//
// Created by Clément on 20/06/2024.
//

#include "config.h"
#include "testutility.h"
#include "libcommon/utility/utility.h"

namespace KDC {

void TestUtility::testGetAppSupportDir() {
    SyncPath appSupportDir = CommonUtility::getAppSupportDir();
    CPPUNIT_ASSERT(!appSupportDir.empty());
#ifdef _WIN32
    CPPUNIT_ASSERT(appSupportDir.string().find("AppData") != std::string::npos);
    CPPUNIT_ASSERT(appSupportDir.string().find("Local") != std::string::npos);
#elif defined(__APPLE__)
    CPPUNIT_ASSERT(appSupportDir.string().find(".config") != std::string::npos);
#else
    CPPUNIT_ASSERT(appSupportDir.string().find("Library") != std::string::npos);
    CPPUNIT_ASSERT(appSupportDir.string().find("Application") != std::string::npos);
#endif
    CPPUNIT_ASSERT(appSupportDir.string().find(APPLICATION_NAME) != std::wstring::npos);
}
}  // namespace KDC