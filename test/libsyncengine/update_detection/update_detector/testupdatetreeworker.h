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

#pragma once

#include "testincludes.h"
#include "update_detection/update_detector/updatetreeworker.h"

using namespace CppUnit;

namespace KDC {

class TestUpdateTreeWorker : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestUpdateTreeWorker);
        CPPUNIT_TEST(testUtilsFunctions);
        CPPUNIT_TEST(testUpdateTmpFileNode);
        CPPUNIT_TEST(testHandleCreateOperationsWithSamePath);
        CPPUNIT_TEST(testSearchForParentNode);
        CPPUNIT_TEST(testGetNewPathAfterMove);
        CPPUNIT_TEST(testStep1);
        CPPUNIT_TEST(testStep2);
        CPPUNIT_TEST(testStep3);
        CPPUNIT_TEST(testStep3b);
        CPPUNIT_TEST(testStep4);
        CPPUNIT_TEST(testStep5);
        CPPUNIT_TEST(testStep6);
        CPPUNIT_TEST(testStep7);
        CPPUNIT_TEST(testStep8);
        CPPUNIT_TEST(testStep8b);
        CPPUNIT_TEST(testClearTreeStep1);
        CPPUNIT_TEST(testClearTreeStep2);
        CPPUNIT_TEST(testClearTreeStep3);
        CPPUNIT_TEST(testClearTreeStep4);
        CPPUNIT_TEST(testClearTreeStep5);
        CPPUNIT_TEST(testClearTreeStep6);
        CPPUNIT_TEST(testClearTreeStep7);
        CPPUNIT_TEST(testClearTreeStep8);
        CPPUNIT_TEST(testGetOriginPath);
        CPPUNIT_TEST(testGetOriginPath2);
        CPPUNIT_TEST(testGetOriginPath3);
        CPPUNIT_TEST(testGetOriginPath4);
        CPPUNIT_TEST(testGetOriginPath5);
        CPPUNIT_TEST(testDeleteMove);
        CPPUNIT_TEST(testDeleteRecreateBranch);
        CPPUNIT_TEST(testIntegrityCheck);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void setUpDbTree();
        void setUpUpdateTree(ReplicaSide side);

        void testUtilsFunctions();
        void testUpdateTmpFileNode();
        void testHandleCreateOperationsWithSamePath();
        void testSearchForParentNode();
        void testGetNewPathAfterMove();

        // Test with already existing UpdateTree
        void testStep1();
        void testStep2();
        void testStep3();
        void testStep3b();
        void testStep4();
        void testStep5();
        void testStep6();
        void testStep7();
        void testStep8();
        void testStep8b();

        // Test with empty UpdateTree
        void testClearTreeStep1();
        void testClearTreeStep2();
        void testClearTreeStep3();
        void testClearTreeStep4();
        void testClearTreeStep5();
        void testClearTreeStep6();
        void testClearTreeStep7();
        void testClearTreeStep8();

        void testGetOriginPath();
        void testGetOriginPath2();
        void testGetOriginPath3();
        void testGetOriginPath4();
        void testGetOriginPath5();

        void testDeleteMove();

        /**
         * Specific test for https://github.com/Infomaniak/desktop-kDrive/pull/176.
         * Let's imagine the following tree structure : A/AA/AAA. The test case is to delete this entire tree structure and
         * re-create it immediately with different IDs but same names.
         */
        void testDeleteRecreateBranch();

        void testIntegrityCheck();

    private:
        std::shared_ptr<UpdateTreeWorker> _localUpdateTreeWorker;
        std::shared_ptr<UpdateTreeWorker> _remoteUpdateTreeWorker;
        std::shared_ptr<SyncDb> _syncDb;
        std::shared_ptr<FSOperationSet> _operationSet;
        std::shared_ptr<UpdateTree> _localUpdateTree;
        std::shared_ptr<UpdateTree> _remoteUpdateTree;

        DbNodeId _dbnodeIdDir41;
        DbNodeId _dbnodeIdDir411;
};
} // namespace KDC
