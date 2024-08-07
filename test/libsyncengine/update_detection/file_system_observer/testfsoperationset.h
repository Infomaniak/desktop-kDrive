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

class TestFsOperationSet : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestFsOperationSet);
        CPPUNIT_TEST(testGetOp);
        CPPUNIT_TEST(testGetOpsByType);
        CPPUNIT_TEST(testGetOpsByNodeId);
        CPPUNIT_TEST(testNbOpsByType);
        CPPUNIT_TEST(testClear);
        CPPUNIT_TEST(testInsertOp);
        CPPUNIT_TEST(testRemoveOp);
        CPPUNIT_TEST(testfindOp);
        CPPUNIT_TEST(testOperatorEqual);
        CPPUNIT_TEST(testCopyConstructor);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testGetOp();
        void testGetOpsByType();
        void testGetOpsByNodeId();
        void testNbOpsByType();
        void testClear();
        void testInsertOp();
        void testRemoveOp();
        void testfindOp();
        void testOperatorEqual();
        void testCopyConstructor();
    private:
        std::vector<OperationType> _operationTypes;
};

}  // namespace KDC
