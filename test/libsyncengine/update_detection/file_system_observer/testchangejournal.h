/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
#include "test_utility/testbase.h"

namespace KDC {

class TestChangeJournal final : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestChangeJournal);
        CPPUNIT_TEST(testCursorRoundTrip);
        CPPUNIT_TEST(testParseCursorRejectsMalformed);
        CPPUNIT_TEST(testReasonFlags);
        CPPUNIT_TEST(testSupportsChangeJournal);
        CPPUNIT_TEST(testCurrentCursor);
        CPPUNIT_TEST(testGetEntriesDetectsChanges);
        CPPUNIT_TEST(testGetEntriesRejectsForeignJournalId);
        CPPUNIT_TEST(testGetEntriesRejectsMalformedCursor);
        CPPUNIT_TEST(testPopulateEntryPath);
        CPPUNIT_TEST(testPopulateEntryPathForDeletedItem);
        CPPUNIT_TEST(testMonitorLive);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() final { TestBase::start(); }
        void tearDown() final { TestBase::stop(); }

    private:
        // Pure logic tests (portable).
        void testCursorRoundTrip();
        void testParseCursorRejectsMalformed();
        void testReasonFlags();

        // Tests exercising the actual NTFS journal (require a working volume).
        void testSupportsChangeJournal();
        void testCurrentCursor();
        void testGetEntriesDetectsChanges();
        void testGetEntriesRejectsForeignJournalId();
        void testGetEntriesRejectsMalformedCursor();
        void testPopulateEntryPath();
        void testPopulateEntryPathForDeletedItem();
        void testMonitorLive();
};

} // namespace KDC
