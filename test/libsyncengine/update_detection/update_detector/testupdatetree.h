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
#include "update_detection/update_detector/updatetreeworker.h"

using namespace CppUnit;

namespace KDC {

class TestUpdateTree : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestUpdateTree);
        CPPUNIT_TEST(testConstructors);
        CPPUNIT_TEST(testAll);
        CPPUNIT_TEST(testChangeEvents);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testConstructors();
        void testAll();
        void testChangeEvents();

    private:
        UpdateTree *_myTree;
        std::shared_ptr<FSOperationSet> _operationSet;
};

}  // namespace KDC
