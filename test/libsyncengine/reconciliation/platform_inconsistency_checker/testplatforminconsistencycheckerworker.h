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

#pragma once

#include "testincludes.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerworker.h"

using namespace CppUnit;

namespace KDC {

class TestPlatformInconsistencyCheckerWorker : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestPlatformInconsistencyCheckerWorker);
        //    CPPUNIT_TEST(testFixNameSize);
        //    CPPUNIT_TEST(testFixNameForbiddenChars);
        //    CPPUNIT_TEST(testFixReservedNames);
        //    CPPUNIT_TEST(testNameClash_noExtension);
        //    CPPUNIT_TEST(testNameClash_withExtension);
        CPPUNIT_TEST(testNameClash_extensionFinishWithDot);
        //    CPPUNIT_TEST(testExecute);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testFixNameSize();
        void testFixNameForbiddenChars();
        void testFixReservedNames();
        void testNameClash_noExtension();
        void testNameClash_withExtension();
        void testNameClash_extensionFinishWithDot();
        void testExecute();

    private:
        std::shared_ptr<SyncPal> _syncPal = nullptr;
};

}  // namespace KDC
