// Infomaniak kDrive - Desktop
// Copyright (C) 2023-2025 Infomaniak Network SA
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include "testincludes.h"
#include "test_classes/testsituationgenerator.h"

namespace KDC {

class SyncDb;
class FSOperationSet;
class UpdateTree;
class UpdateTreeWorker;

class BenchUpdateTreeWorker final : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(BenchUpdateTreeWorker);
        CPPUNIT_TEST(measureUpdateTreeGenerationFromScratch);
        CPPUNIT_TEST(measureUpdateTreeGenerationFromExisting);
        CPPUNIT_TEST(measureUpdateTreeGenerationFromScratchWithFsOps);
        CPPUNIT_TEST(measureUpdateTreeGenerationFromExistingWithFsOps);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    private:
        void measureUpdateTreeGenerationFromScratch();
        void measureUpdateTreeGenerationFromExisting();
        void measureUpdateTreeGenerationFromScratchWithFsOps();
        void measureUpdateTreeGenerationFromExistingWithFsOps();

        std::shared_ptr<UpdateTreeWorker> _testObj;
        std::shared_ptr<UpdateTree> _updateTree;
        std::shared_ptr<FSOperationSet> _fsOpSet;
        std::shared_ptr<SyncDb> _syncDb;
        TestSituationGenerator _situationGenerator;
};
} // namespace KDC
