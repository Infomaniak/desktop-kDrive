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
#include "server/updater/abstractupdater.h"

namespace KDC {

class UpdaterTest final : public AbstractUpdater {
    public:
        void startInstaller() override {}
        void onUpdateFound() override {}
};

class TestAbstractUpdater final : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestAbstractUpdater);
        CPPUNIT_TEST(testSkipUnskipVersion);
        CPPUNIT_TEST(testIsVersionSkipped);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;

    protected:
        void testSkipUnskipVersion() const;
        void testIsVersionSkipped() const;

    private:
        UpdaterTest _testObj;
};

} // namespace KDC
