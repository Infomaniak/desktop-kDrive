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

#include "config.h"
#include "testsentryhandler.h"
#include "libcommon/log/sentry/sentryhandler.h"
#include "libcommonserver/log/log.h"

#include <thread>

namespace KDC {
MockTestSentryHandler::MockTestSentryHandler(): SentryHandler() {
    SentryHandler::setIsSentryActivated(true);
    SentryHandler::setMaxCaptureCountBeforeRateLimit(3);
    SentryHandler::setMinUploadIntervalOnRateLimit(1);
}
void MockTestSentryHandler::sendEventToSentry(const SentryLevel level, const std::string& title,
                                              const std::string& message) const {
    _sentryUploadedEventCount++;
}


void TestSentryHandler::testMultipleSendEventForTheSameEvent() {
    MockTestSentryHandler mockSentryHandler;

    CPPUNIT_ASSERT_EQUAL(0, mockSentryHandler.sentryUploadedEventCount());
    mockSentryHandler.captureMessage(SentryLevel::Info, "Test", "Test message"); // Should be sent
    CPPUNIT_ASSERT_EQUAL(1, mockSentryHandler.sentryUploadedEventCount());
    mockSentryHandler.captureMessage(SentryLevel::Info, "Test", "Test message"); // Rate limit not reached, should be sent
    CPPUNIT_ASSERT_EQUAL(2, mockSentryHandler.sentryUploadedEventCount());
    mockSentryHandler.captureMessage(SentryLevel::Info, "Test",
                                     "Test message"); // Rate limit reached, should be sent (first event on rate limit is sent).
    CPPUNIT_ASSERT_EQUAL(3, mockSentryHandler.sentryUploadedEventCount());

    for (int i = 3; i <= 4; i++) { // Done twice to check that the timer is well reset.
        mockSentryHandler.captureMessage(SentryLevel::Info, "Test", "Test message"); // Rate limit reached, should not be sent
        CPPUNIT_ASSERT_EQUAL(i, mockSentryHandler.sentryUploadedEventCount());

        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait less than upload rate limit (1s in test)

        mockSentryHandler.captureMessage(SentryLevel::Info, "Test", "Test message"); // Rate limit reached, should not be sent
        CPPUNIT_ASSERT_EQUAL(i, mockSentryHandler.sentryUploadedEventCount());

        std::this_thread::sleep_for(std::chrono::milliseconds(600)); // Wait 600ms more, the upload rate limit should be passed

        mockSentryHandler.captureMessage(SentryLevel::Info, "Test",
                                         "Test message"); // Should be sent
        CPPUNIT_ASSERT_EQUAL(i + 1, mockSentryHandler.sentryUploadedEventCount());
    }

    std::this_thread::sleep_for(
            std::chrono::seconds(1)); // Wait 1s without capturing any event. This should reset all rate limits.

    mockSentryHandler.captureMessage(SentryLevel::Info, "Test", "Test message"); // Rate limit not reached, should be sent
    CPPUNIT_ASSERT_EQUAL(6, mockSentryHandler.sentryUploadedEventCount());
    mockSentryHandler.captureMessage(SentryLevel::Info, "Test", "Test message"); // Rate limit not reached, should be sent
    CPPUNIT_ASSERT_EQUAL(7, mockSentryHandler.sentryUploadedEventCount());
    mockSentryHandler.captureMessage(SentryLevel::Info, "Test",
                                     "Test message"); // Rate limit reached, should be sent (first event on rate limit is sent).
    CPPUNIT_ASSERT_EQUAL(8, mockSentryHandler.sentryUploadedEventCount());

    mockSentryHandler.captureMessage(SentryLevel::Info, "Test", "Test message"); // Rate limit reached, should not be sent
    CPPUNIT_ASSERT_EQUAL(8, mockSentryHandler.sentryUploadedEventCount()); 
}
void TestSentryHandler::testMultipleSendEventForDifferentEvent() {
    MockTestSentryHandler mockSentryHandler;

    // Test all levels
    CPPUNIT_ASSERT_EQUAL(0, mockSentryHandler.sentryUploadedEventCount());

    mockSentryHandler.captureMessage(SentryLevel::Info, "Test", "Test message"); // Should be sent
    CPPUNIT_ASSERT_EQUAL(1, mockSentryHandler.sentryUploadedEventCount());

    mockSentryHandler.captureMessage(SentryLevel::Debug, "Test", "Test message"); // Should be sent
    CPPUNIT_ASSERT_EQUAL(2, mockSentryHandler.sentryUploadedEventCount());

    mockSentryHandler.captureMessage(SentryLevel::Warning, "Test", "Test message"); // Should be sent
    CPPUNIT_ASSERT_EQUAL(3, mockSentryHandler.sentryUploadedEventCount());

    mockSentryHandler.captureMessage(SentryLevel::Fatal, "Test", "Test message"); // Should be sent
    CPPUNIT_ASSERT_EQUAL(4, mockSentryHandler.sentryUploadedEventCount());

    //Test Title change
    mockSentryHandler.captureMessage(SentryLevel::Info, "Test2", "Test message"); // Should be sent
    CPPUNIT_ASSERT_EQUAL(5, mockSentryHandler.sentryUploadedEventCount());

    mockSentryHandler.captureMessage(SentryLevel::Info, "Test3", "Test message"); // Should be sent
    CPPUNIT_ASSERT_EQUAL(6, mockSentryHandler.sentryUploadedEventCount());

    mockSentryHandler.captureMessage(SentryLevel::Info, "Test4", "Test message"); // Should be sent
    CPPUNIT_ASSERT_EQUAL(7, mockSentryHandler.sentryUploadedEventCount());

    mockSentryHandler.captureMessage(SentryLevel::Info, "Test5", "Test message"); // Should be sent
    CPPUNIT_ASSERT_EQUAL(8, mockSentryHandler.sentryUploadedEventCount());

    //Test Message change
    mockSentryHandler.captureMessage(SentryLevel::Info, "Test", "Test message2"); // Should be sent
    CPPUNIT_ASSERT_EQUAL(9, mockSentryHandler.sentryUploadedEventCount());

    mockSentryHandler.captureMessage(SentryLevel::Info, "Test", "Test message3"); // Should be sent
    CPPUNIT_ASSERT_EQUAL(10, mockSentryHandler.sentryUploadedEventCount());

    mockSentryHandler.captureMessage(SentryLevel::Info, "Test", "Test message4"); // Should be sent
    CPPUNIT_ASSERT_EQUAL(11, mockSentryHandler.sentryUploadedEventCount());

    mockSentryHandler.captureMessage(SentryLevel::Info, "Test", "Test message5"); // Should be sent
    CPPUNIT_ASSERT_EQUAL(12, mockSentryHandler.sentryUploadedEventCount());
}
} // namespace KDC