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
#include "syncpal/syncpal.h"

using namespace CppUnit;

namespace KDC {

class TestSnapshot : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestSnapshot);
        CPPUNIT_TEST(testSnapshot);
        CPPUNIT_TEST(testDuplicatedItem);
        CPPUNIT_TEST(testSnapshotInsertionWithDifferentEncodings);
        CPPUNIT_TEST(testPath);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    private:
        void testSnapshot();
        void testDuplicatedItem();
        void testSnapshotInsertionWithDifferentEncodings();
        void testPath();

        std::unique_ptr<Snapshot> _snapshot;
        NodeId _rootNodeId;
};

} // namespace KDC
