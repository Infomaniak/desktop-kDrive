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

namespace KDC {
void MockTestSentryHandler::sendEventToSentry(const SentryLevel level, const std::string& title,
                                              const std::string& message) const {
    std::cout << "MockTestSentryHandler::sendEventToSentry: " << title << " - " << message << std::endl;
    _sentrySendedEventCount++;
}


void TestSentryHandler::testMultipleSendEventForTheSameEvent() {
    MockTestSentryHandler mockSentryHandler;

    CPPUNIT_ASSERT_EQUAL(0, mockSentryHandler.sentrySendedEventCount());
    mockSentryHandler.captureMessage(SentryLevel::Fatal, "Fatal", "Fatal message"); // Should be sent
    CPPUNIT_ASSERT_EQUAL(1, mockSentryHandler.sentrySendedEventCount());
    mockSentryHandler.captureMessage(SentryLevel::Fatal, "Fatal", "Fatal message"); // Rate limit not reached, should be sent
    CPPUNIT_ASSERT_EQUAL(2, mockSentryHandler.sentrySendedEventCount());
    mockSentryHandler.captureMessage(SentryLevel::Fatal, "Fatal",
                                     "Fatal message"); // Rate limit reached, should be sent (first event on rate limit is sent).
    CPPUNIT_ASSERT_EQUAL(3, mockSentryHandler.sentrySendedEventCount());

    for (int i = 3; i <= 4; i++) { // Done twice to check that the timer is well reset.
        mockSentryHandler.captureMessage(SentryLevel::Fatal, "Fatal", "Fatal message"); // Rate limit reached, should not be sent
        CPPUNIT_ASSERT_EQUAL(i, mockSentryHandler.sentrySendedEventCount());

        std::this_thread::sleep_for(std::chrono::seconds(2)); // Wait less than upload rate limit (3s in test)

        mockSentryHandler.captureMessage(SentryLevel::Fatal, "Fatal", "Fatal message"); // Rate limit reached, should not be sent
        CPPUNIT_ASSERT_EQUAL(i, mockSentryHandler.sentrySendedEventCount());

        std::this_thread::sleep_for(std::chrono::seconds(2)); // Wait 2s more, the upload rate limit should be passed

        mockSentryHandler.captureMessage(SentryLevel::Fatal, "Fatal",
                                         "Fatal message"); // Should be sent
        CPPUNIT_ASSERT_EQUAL(i + 1, mockSentryHandler.sentrySendedEventCount());
    }

    std::this_thread::sleep_for(
            std::chrono::seconds(4)); // Wait 4s witout capturing any event, it should be reset all rate limits

    mockSentryHandler.captureMessage(SentryLevel::Fatal, "Fatal", "Fatal message"); // Rate limit not reached, should be sent
    CPPUNIT_ASSERT_EQUAL(6, mockSentryHandler.sentrySendedEventCount());
    mockSentryHandler.captureMessage(SentryLevel::Fatal, "Fatal", "Fatal message"); // Rate limit not reached, should be sent
    CPPUNIT_ASSERT_EQUAL(7, mockSentryHandler.sentrySendedEventCount());
    mockSentryHandler.captureMessage(SentryLevel::Fatal, "Fatal",
                                     "Fatal message"); // Rate limit reached, should be sent (first event on rate limit is sent).
    CPPUNIT_ASSERT_EQUAL(8, mockSentryHandler.sentrySendedEventCount());

    mockSentryHandler.captureMessage(SentryLevel::Fatal, "Fatal", "Fatal message"); // Rate limit reached, should not be sent
    CPPUNIT_ASSERT_EQUAL(8, mockSentryHandler.sentrySendedEventCount()); 
}
} // namespace KDC
