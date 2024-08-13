
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

< < < < < < < < HEAD : test / test_utility /
                       testhelpers.h
#include "utility/types.h"

                       namespace KDC::testhelpers {
    SyncName makeNfdSyncName();
    SyncName makeNfcSyncName();

}  // namespace KDC::testhelpers
== == == ==
#include "testincludes.h"
#include "server/updater_v2/abstractupdater.h"
#include "utility/types.h"
    using namespace CppUnit;

namespace KDC {
class TestAbstractUpdater : public CppUnit::TestFixture {
    public:
        CPPUNIT_TEST_SUITE(TestAbstractUpdater);
        CPPUNIT_TEST(testCheckUpdateAvailable);
        CPPUNIT_TEST(testCurrentVersion);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testCheckUpdateAvailable();
        void testCurrentVersion();
};
}  // namespace KDC
>>>>>>>> 37bcbb1e65f30900cafbb83e6cff12156a7a3dfe : test / server / updater / testabstractupdater.h
