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

#include "testincludes.h"

#include "api_token/testapitoken.h"
#include "utility/testutility.h"
#include "utility/testtypes.h"
#include "log/sentry/testsentryhandler.h"
#include "utility/testurlhelper.h"
#include "utility/testjsonparserutility.h"

namespace KDC {
CPPUNIT_TEST_SUITE_REGISTRATION(TestApiToken);
CPPUNIT_TEST_SUITE_REGISTRATION(TestUtility);
CPPUNIT_TEST_SUITE_REGISTRATION(TestTypes);
CPPUNIT_TEST_SUITE_REGISTRATION(TestSentryHandler);
CPPUNIT_TEST_SUITE_REGISTRATION(TestUrlHelper);
CPPUNIT_TEST_SUITE_REGISTRATION(TestJsonParserUtility);
} // namespace KDC

int main(int, char **) {
    return runTestSuite("_kDriveTestCommon.log");
}
