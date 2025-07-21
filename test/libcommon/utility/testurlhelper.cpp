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

#include "testurlhelper.h"

#include "libcommon/utility/utility.h"
#include "utility/urlhelper.h"
#include "utility/utility.h"

namespace KDC {

static const std::string preprodKeyWord = "preprod";

void TestUrlHelper::testGetUrl() {
    // Because UrlHelper evaluate KDRIVE_USE_PREPROD_URL only once and store it in a static variable, we cannot switch between
    // prod and preprod URLs. Therefore, we randomly test either prod or preprod.
    std::random_device rd;
    std::default_random_engine gen(rd());
    std::uniform_int_distribution distrib(1, 2);
    const auto test = distrib(gen);
    const bool usePreprod = test == 1;
    if (usePreprod) {
#if defined(KD_WINDOWS)
        _putenv_s("KDRIVE_USE_PREPROD_URL", "1");
#else
        (void) setenv("KDRIVE_USE_PREPROD_URL", "1", true);
#endif
        std::cout << " Testing preprod URLs";
    } else {
#if defined(KD_WINDOWS)
        _putenv_s("KDRIVE_USE_PREPROD_URL", "0");
#else
        (void) setenv("KDRIVE_USE_PREPROD_URL", "0", true);
#endif
        std::cout << " Testing prod URLs";
    }


    CPPUNIT_ASSERT_EQUAL(usePreprod, Utility::contains(UrlHelper::infomaniakApiUrl(), preprodKeyWord));
    CPPUNIT_ASSERT_EQUAL(false, Utility::contains(UrlHelper::infomaniakApiUrl(2, true), preprodKeyWord));
    CPPUNIT_ASSERT_EQUAL(usePreprod, Utility::contains(UrlHelper::kDriveApiUrl(), preprodKeyWord));
    CPPUNIT_ASSERT_EQUAL(usePreprod, Utility::contains(UrlHelper::notifyApiUrl(), preprodKeyWord));
    CPPUNIT_ASSERT_EQUAL(usePreprod, Utility::contains(UrlHelper::loginApiUrl(), preprodKeyWord));

    CPPUNIT_ASSERT_EQUAL(true, Utility::endsWith(UrlHelper::infomaniakApiUrl(), "/2"));
    CPPUNIT_ASSERT_EQUAL(true, Utility::endsWith(UrlHelper::infomaniakApiUrl(123), "/123"));
    CPPUNIT_ASSERT_EQUAL(true, Utility::endsWith(UrlHelper::infomaniakApiUrl(123, true), "/123"));
    CPPUNIT_ASSERT_EQUAL(true, Utility::endsWith(UrlHelper::kDriveApiUrl(), "/2"));
    CPPUNIT_ASSERT_EQUAL(true, Utility::endsWith(UrlHelper::kDriveApiUrl(123), "/123"));
    CPPUNIT_ASSERT_EQUAL(true, Utility::endsWith(UrlHelper::notifyApiUrl(), "/2"));
    CPPUNIT_ASSERT_EQUAL(true, Utility::endsWith(UrlHelper::notifyApiUrl(123), "/123"));
}

} // namespace KDC
