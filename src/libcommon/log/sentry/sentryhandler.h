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

#include <sentry.h>
#include <string>
#include <mutex>
#include <unordered_map>

#include "utility/types.h"
#include "sentryuser.h"

namespace KDC {

enum class SentryLevel { // Not defined in types.h as we don't want to include sentry.h in types.h
    Debug = SENTRY_LEVEL_DEBUG,
    Info = SENTRY_LEVEL_INFO,
    Warning = SENTRY_LEVEL_WARNING,
    Error = SENTRY_LEVEL_ERROR,
    Fatal = SENTRY_LEVEL_FATAL
};

inline std::string toString(SentryLevel level) {
    switch (level) {
        case SentryLevel::Debug:
            return "Debug";
        case SentryLevel::Info:
            return "Info";
        case SentryLevel::Warning:
            return "Warning";
        case SentryLevel::Error:
            return "Error";
        case SentryLevel::Fatal:
            return "Fatal";
    }
};

class SentryHandler {
    public:
        enum class SentryProject { Server, Client, Deactivated }; // Only used for initialization, don't need to be in types.h

        virtual ~SentryHandler();
        static std::shared_ptr<SentryHandler> instance();
        static void init(SentryProject project, int breadCrumbsSize = 100);
        void setAuthenticatedUser(const SentryUser &user);
        void setGlobalConfidentialityLevel(SentryConfidentialityLevel level);
        /*   If the same event has been captured more than 10 times in the last 10 minutes, it will be flagged as a rate limited
         *     event.
         *   If a rate limited event is not seen for 10 minutes, it will be unflagged.
         *
         *   Rate limited events:
         *   - Will see their level escalated to Error
         *   - Will be upload to Sentry only if the last upload is older than 10 minutes else it will just increment the capture
         *      count.
         *   - The event sent to Sentry will have a message indicating that the event has been rate limited.(see:
         *      void SentryHandler::escalateSentryEvent(SentryEvent &event))
         */
        void captureMessage(SentryLevel level, const std::string &title, std::string message,
                            const SentryUser &user = SentryUser());

    protected:
        SentryHandler() = default;
        void setMaxCaptureCountBeforeRateLimit(int maxCaptureCountBeforeRateLimit);
        void setMinUploadIntervalOnRateLimit(int minUploadIntervalOnRateLimit);
        void setIsSentryActivated(bool isSentryActivated) { _isSentryActivated = isSentryActivated; }
        virtual void sendEventToSentry(const SentryLevel level, const std::string &title, const std::string &message) const;

    private:
        SentryHandler(const SentryHandler &) = delete;
        SentryHandler &operator=(const SentryHandler &) = delete;
        SentryHandler(SentryHandler &&) = delete;
        SentryHandler &operator=(SentryHandler &&) = delete;

        sentry_value_t toSentryValue(const SentryUser &user) const;

        struct SentryEvent {
                using time_point = std::chrono::system_clock::time_point;

                SentryEvent(const std::string &title, const std::string &message, SentryLevel level,
                            SentryConfidentialityLevel userType, const SentryUser &user);
                std::string getStr() const { return title + message + static_cast<char>(level) + userId; };
                std::string title;
                std::string message;
                SentryLevel level;
                SentryConfidentialityLevel confidentialityLevel = SentryConfidentialityLevel::Anonymous;
                std::string userId;
                time_point lastCapture;
                time_point lastUpload;
                unsigned int captureCount = 1;
        };

        /* This method is called before uploading an event to Sentry.
         *  It will check if the event should be uploaded or not. (see: void SentryHandler::captureMessage(...))
         *  The event passed as parameter will be updated if it is rate limited (level and message).
         */
        void handleEventsRateLimit(SentryEvent &event, bool &toUpload);

        // Return true if last capture is older than minUploadInterval
        bool lastEventCaptureIsOutdated(const SentryEvent &event) const;
        // Return true if last upload is older than minUploadInterval
        bool lastEventUploadIsOutdated(const SentryEvent &event) const;

        // Escalate a sentry event. The event level will be set to Error and the message will be updated to indicate that the
        // event has been rate limited.
        void escalateSentryEvent(SentryEvent &event);

        /* Update the effective sentry user.
        If authentication type is Anonymous, the user parameter will be ignored and the
        effective user will be set to Anonymous.
        If authentication type is Authenticated, the user parameter will be used.
        If no user is provided (the user provided is the default user), the effective user will be set to Anonymous.
        */
        void updateEffectiveSentryUser(const SentryUser &user = SentryUser());

        std::recursive_mutex _mutex;
        static std::shared_ptr<SentryHandler> _instance;
        SentryUser _authenticatedUser;
        struct StringHash {
                using is_transparent = void; // Enables heterogeneous operations.
                std::size_t operator()(std::string_view sv) const {
                    std::hash<std::string_view> hasher;
                    return hasher(sv);
                }
        };

        std::unordered_map<std::string, SentryEvent, StringHash, std::equal_to<>> _events;
        SentryConfidentialityLevel _globalConfidentialityLevel = SentryConfidentialityLevel::Anonymous; // Default value
        SentryConfidentialityLevel _lastConfidentialityLevel = SentryConfidentialityLevel::None;
        int _sentryMaxCaptureCountBeforeRateLimit = 10; // Number of capture before rate limiting an event
        int _sentryMinUploadIntervaOnRateLimit = 60; // Min. interval between two uploads of a rate limited event (seconds)
        bool _isSentryActivated = false;
};
} // namespace KDC
