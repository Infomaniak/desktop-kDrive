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

#include <sentry.h>
#include <string>
#include <mutex>
#include <unordered_map>

#include "libcommon/utility/types.h"
#include "user.h"
#include "ptrace.h"
#include "ptracedescriptor.h"

namespace KDC {
namespace sentry {
enum class Level { // Not defined in types.h as we don't want to include sentry.h in types.h
    Debug = SENTRY_LEVEL_DEBUG,
    Info = SENTRY_LEVEL_INFO,
    Warning = SENTRY_LEVEL_WARNING,
    Error = SENTRY_LEVEL_ERROR,
    Fatal = SENTRY_LEVEL_FATAL
};

// The final status of a performance trace.
enum class PTraceStatus {
    Ok = SENTRY_SPAN_STATUS_OK,
    Cancelled = SENTRY_SPAN_STATUS_CANCELLED, // The operation hasn't terminated but should not be considered as a faillure.
    Aborted = SENTRY_SPAN_STATUS_ABORTED, // The operation failed.
};
} // namespace sentry
inline std::string toString(sentry::Level level) {
    switch (level) {
        case sentry::Level::Debug:
            return "Debug";
        case sentry::Level::Info:
            return "Info";
        case sentry::Level::Warning:
            return "Warning";
        case sentry::Level::Error:
            return "Error";
        case sentry::Level::Fatal:
            return "Fatal";
        default:
            return "No conversion to string available";
    }
};

namespace sentry {
class Handler {
    public:
        virtual ~Handler();
        static std::shared_ptr<Handler> instance();
        static void init(AppType appType, int breadCrumbsSize = 100);
        void setAuthenticatedUser(const SentryUser &user);
        void setGlobalConfidentialityLevel(sentry::ConfidentialityLevel level);

        // Capture an event
        /*   If the same event has been captured more than 10 times in the last 10 minutes, it will be flagged as a rate
         * limited event. If a rate limited event is not seen for 10 minutes, it will be unflagged.
         *
         *   Rate limited events:
         *   - Will see their level escalated to Error
         *   - Will be upload to Sentry only if the last upload is older than 10 minutes else it will just increment the
         * capture count.
         *   - The event sent to Sentry will have a message indicating that the event has been rate limited.(see:
         *      void Handler::escalateSentryEvent(SentryEvent &event))
         */
        static void captureMessage(Level level, const std::string &title, const std::string &message,
                                   const SentryUser &user = SentryUser()) {
            instance()->privateCaptureMessage(level, title, message, user);
        }


        // Performances monitoring
        bool arePtracesEnabled() const;
        pTraceId startPTrace(const PTraceDescriptor &pTraceInfo, int syncDbId = -1);
        void stopPTrace(const PTraceDescriptor &pTraceInfo, int syncDbId = -1, PTraceStatus status = PTraceStatus::Ok);
        void stopPTrace(const pTraceId &pTraceId, PTraceStatus status = PTraceStatus::Ok);

        // Debugging
        inline static AppType appType() { return _appType; }
        inline static bool debugCrashCallback() { return _debugCrashCallback; }
        inline static bool debugBeforeSendCallback() { return _debugBeforeSendCallback; }

        // Print an event description into a file (for debugging)
        static void writeEvent(const std::string &eventStr) noexcept { writeEvent(eventStr, false); };
        // Print a crash event description into a file (for debugging)
        static void writeCrashEvent(const std::string &eventStr) noexcept { writeEvent(eventStr, true); };

        void setDistributionChannel(VersionChannel channel);
        void setAppUUID(std::string appUUID);

        // Returns the file path where non-crash events of type `appType` are written locally.
        static SyncPath getEventFilePath(const AppType appType) { return getEventFilePath(appType, false); };
        // Returns the file path where crash events of type `appType` are written locally.
        static SyncPath getCrashEventFilePath(const AppType appType) { return getEventFilePath(appType, true); };

    protected:
        Handler() = default;
        void setMaxCaptureCountBeforeRateLimit(int maxCaptureCountBeforeRateLimit);
        void setMinUploadIntervalOnRateLimit(int minUploadIntervalOnRateLimit);
        void setIsSentryActivated(bool isSentryActivated) { _isSentryActivated = isSentryActivated; }
        virtual void sendEventToSentry(const Level level, const std::string &title, const std::string &message) const;
        void privateCaptureMessage(Level level, const std::string &title, std::string message,
                                   const SentryUser &user = SentryUser());


    private:
        Handler(const Handler &) = delete;
        Handler &operator=(const Handler &) = delete;
        Handler(Handler &&) = delete;
        Handler &operator=(Handler &&) = delete;

        std::recursive_mutex _mutex;
        static std::shared_ptr<Handler> _instance;
        bool _isSentryActivated = false;

        // The user that will be used to send general events to Sentry. This variable can be updated with
        // `setAuthenticatedUser(...)`
        SentryUser _authenticatedUser;

        sentry::ConfidentialityLevel _globalConfidentialityLevel = sentry::ConfidentialityLevel::Anonymous; // Default value
        sentry::ConfidentialityLevel _lastConfidentialityLevel = sentry::ConfidentialityLevel::None;

        void setTag(const std::string &key, const std::string &value);
        void removeTag(const std::string &key) { sentry_remove_tag(key.c_str()); }
        // Convert a `SentryUser` structure to a `sentry_value_t` that can safely be passed to
        // `sentry_set_user(sentry_value_t)`
        [[nodiscard]] sentry_value_t toSentryValue(const SentryUser &user) const;

        // SentryEvent is a structure that represents an event that has been send to sentry. It allows us to keep track of
        // the sent event and block any sentry flood from a single user.
        struct SentryEvent {
                using time_point = std::chrono::system_clock::time_point;

                SentryEvent(const std::string &title, const std::string &message, Level level,
                            sentry::ConfidentialityLevel userType, const SentryUser &user);
                [[nodiscard]] std::string getStr() const { return title + message + static_cast<char>(level) + userId; }
                std::string title;
                std::string message;
                Level level;
                sentry::ConfidentialityLevel confidentialityLevel = sentry::ConfidentialityLevel::Anonymous;
                std::string userId;
                time_point lastCapture;
                time_point lastUpload;
                unsigned int captureCount = 1;
        };

        /* This method is called before uploading an event to Sentry.
         *  It will check if the event should be uploaded or not. (see: void Handler::captureMessage(...))
         *  The event passed as parameter will be updated if it is rate limit is reached (level and message).
         */
        void handleEventsRateLimit(SentryEvent &event, bool &toUpload);

        // Return true if last capture of a similar event is older than minUploadInterval
        bool lastEventCaptureIsOutdated(const SentryEvent &event) const;

        // Return true if last upload of a similar event is older than minUploadInterval
        bool lastEventUploadIsOutdated(const SentryEvent &event) const;

        // Escalate a sentry event. The event level will be set to Error and the message will be updated to indicate that
        // the event has been rate limited.
        void escalateSentryEvent(SentryEvent &event) const;

        /* Update the effective sentry user.
         * - If authentication type is `anonymous`, the user parameter will be ignored and the
         * effective user will be set to Anonymous.
         * - If `user` parameter is not provided, the effective user will be set to `Anonymous`.
         * - If authentication type is `authenticated` and `user` parameter is provided, the user parameter will be used.
         */
        void updateEffectiveSentryUser(const SentryUser &user = SentryUser());

        // The events that have been recently captured, used to prevent flood from a single user.
        std::unordered_map<std::string, SentryEvent, StringHashFunction, std::equal_to<>> _events;

        // Number of captures before rate limiting an event
        unsigned int _sentryMaxCaptureCountBeforeRateLimit = 10;

        // Min. interval between two uploads of a rate limited event (seconds)
        int _sentryMinUploadIntervalOnRateLimit = 60;

        pTraceId _pTraceIdCounter = 1;
        // Generate a unique pTraceId. Automatically reset to 1 when reaching the maximum value.
        pTraceId makeUniquePTraceId();
        std::map<pTraceId, PerformanceTrace> _pTraces;

        // A transaction allows to track the duration of an operation. It cannot
        // be a child of another transaction.
        pTraceId startTransaction(const std::string &name, const std::string &description);

        // A Span works as a transaction, but it needs to have a parent
        // (which can be either a transaction or another span).
        pTraceId startSpan(const std::string &name, const std::string &description, const pTraceId &parentId);

        std::map<int /*syncDbId*/, std::map<PTraceName, pTraceId>> _pTraceNameToPTraceIdMap;

        bool checkCustomSampleRate(const PTraceDescriptor &pTraceInfo) const;

        static void writeEvent(const std::string &eventStr, bool crash) noexcept;

        // The temporary directory where Sentry events are written.
        static SyncPath getSentryTemporaryDir();

        // Returns the file path where events of type `appType` are written locally.
        static SyncPath getEventFilePath(AppType appType, bool crash);

        // Debug
        static AppType _appType;
        static bool _debugCrashCallback;
        static bool _debugBeforeSendCallback;
};
} // namespace sentry
} // namespace KDC
