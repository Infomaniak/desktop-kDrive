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
#include "libcommon/log/sentry/sentryhandler.h"

namespace KDC {

class MockTestSentryHandler : public SentryHandler {
    public:
        MockTestSentryHandler() : SentryHandler(3, 1){}; // Max 3 events per second
        int sentryUploadedEventCount() const { return _sentryUploadedEventCount; }

    private:
        void sendEventToSentry(const SentryLevel level, const std::string &title, const std::string &message) const final;
        mutable int _sentryUploadedEventCount = 0;
};


class TestSentryHandler : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestSentryHandler);
        CPPUNIT_TEST(testMultipleSendEventForTheSameEvent);
        CPPUNIT_TEST(testMultipleSendEventForDifferentEvent);
        CPPUNIT_TEST_SUITE_END();

    protected:
        void testMultipleSendEventForTheSameEvent();
        void testMultipleSendEventForDifferentEvent();
};
} // namespace KDC
