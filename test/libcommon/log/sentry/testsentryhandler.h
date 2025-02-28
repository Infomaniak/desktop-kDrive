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

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "libcommon/log/sentry/handler.h"
#include "test_utility/testbase.h"

namespace KDC {

class MockTestSentryHandler : public sentry::Handler {
    public:
        MockTestSentryHandler();
        int sentryUploadedEventCount() const { return _sentryUploadedEventCount; }
        void captureMessage(sentry::Level level, const std::string &title, const std::string &message,
                            const SentryUser &user = SentryUser()) {
            privateCaptureMessage(level, title, message, user);
        }

    private:
        void sendEventToSentry(const sentry::Level level, const std::string &title, const std::string &message) const final;
        mutable int _sentryUploadedEventCount = 0;
};


class TestSentryHandler : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestSentryHandler);
        CPPUNIT_TEST(testMultipleSendEventForTheSameEvent);
        CPPUNIT_TEST(testMultipleSendEventForDifferentEvent);
        CPPUNIT_TEST(testWriteEvent);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp(void) override { TestBase::start(); }
        void tearDown(void) override { TestBase::stop(); }

    protected:
        void testMultipleSendEventForTheSameEvent();
        void testMultipleSendEventForDifferentEvent();
        void testWriteEvent();
};
} // namespace KDC
