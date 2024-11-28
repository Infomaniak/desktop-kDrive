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

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "libcommon/log/sentry/handler.h"

namespace KDC {

class MockTestSentryHandler : public Sentry::Handler {
    public:
        MockTestSentry::Handler();
        int sentryUploadedEventCount() const { return _sentryUploadedEventCount; }

    private:
        void sendEventToSentry(const Sentry::Level level, const std::string &title, const std::string &message) const final;
        mutable int _sentryUploadedEventCount = 0;
};


class TestSentry::Handler : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestSentry::Handler);
        CPPUNIT_TEST(testMultipleSendEventForTheSameEvent);
        CPPUNIT_TEST(testMultipleSendEventForDifferentEvent);
        CPPUNIT_TEST(testWriteEvent);
        CPPUNIT_TEST_SUITE_END();

    protected:
        void testMultipleSendEventForTheSameEvent();
        void testMultipleSendEventForDifferentEvent();
        void testWriteEvent();
};
} // namespace KDC
