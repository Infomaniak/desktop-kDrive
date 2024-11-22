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

#include "libcommon/utility/types.h"
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
        default:
            return "No conversion to string available";
    }
};

class SentryHandler {
    public:
        // Each PTraceName need to be defined in PTraceInfo constructor.
        enum class PTraceName {
            None,
            AppStart,
            SyncInit,
            /*SyncInit*/ ResetStatus,
            Sync,
            /*Sync*/ UpdateDetection1,
            /*Sync*/ /*UpdateDetection1*/ UpdateUnsyncedList,
            /*Sync*/ /*UpdateDetection1*/ InferChangesFromDb,
            /*Sync*/ /*UpdateDetection1*/ ExploreLocalSnapshot,
            /*Sync*/ /*UpdateDetection1*/ ExploreRemoteSnapshot,
            /*Sync*/ UpdateDetection2,
            /*Sync*/ /*UpdateDetection2*/ ResetNodes,
            /*Sync*/ /*UpdateDetection2*/ Step1MoveDirectory,
            /*Sync*/ /*UpdateDetection2*/ Step2MoveFile,
            /*Sync*/ /*UpdateDetection2*/ Step3DeleteDirectory,
            /*Sync*/ /*UpdateDetection2*/ Step4DeleteFile,
            /*Sync*/ /*UpdateDetection2*/ Step5CreateDirectory,
            /*Sync*/ /*UpdateDetection2*/ Step6CreateFile,
            /*Sync*/ /*UpdateDetection2*/ Step7EditFile,
            /*Sync*/ /*UpdateDetection2*/ Step8CompleteUpdateTree,
            /*Sync*/ Reconciliation1,
            /*Sync*/ /*Reconciliation1*/ CheckLocalTree,
            /*Sync*/ /*Reconciliation1*/ CheckRemoteTree,
            /*Sync*/ Reconciliation2,
            /*Sync*/ Reconciliation3,
            /*Sync*/ Reconciliation4,
            /*Sync*/ /*Reconciliation4*/ GenerateItemOperations,
            /*Sync*/ Propagation1,
            /*Sync*/ Propagation2,
            /*Sync*/ /*Propagation2*/ InitProgress,
            /*Sync*/ /*Propagation2*/ JobGeneration,
            /*Sync*/ /*Propagation2*/ waitForAllJobsToFinish,
            LFSO_GenerateInitialSnapshot,
            /*LFSO_GenerateInitialSnapshot*/ LFSO_ExploreItem,
            LFSO_ChangeDetected,
            RFSO_GenerateInitialSnapshot,
            /*RFSO_GenerateInitialSnapshot*/ RFSO_BackRequest,
            /*RFSO_GenerateInitialSnapshot*/ RFSO_ExploreItem,
            RFSO_ChangeDetected,
        };

        // A ScopedPTrace object will start a Performance Trace and stop it when it is destroyed.
        struct ScopedPTrace {
                // Start a performance trace, if the trace is not stopped when the object is destroyed, it will be stopped as a
                // failure or success depending on the `manualStopExpected` parameter.
                ScopedPTrace(const SentryHandler::PTraceName &pTraceName, int syncDbId = -1, bool manualStopExpected = false) :
                    _manualStopExpected(manualStopExpected) {
                    _pTraceId = SentryHandler::instance()->startPTrace(pTraceName, syncDbId);
                }

                // If the performance trace with the given id is not stopped when the object is destroyed, it will be stopped as a
                // failure.
                explicit ScopedPTrace(const pTraceId &transactionId, bool manualStopExpected = false) :
                    _pTraceId(transactionId), _manualStopExpected(manualStopExpected) {}
                ~ScopedPTrace() { stop(_manualStopExpected); }

                // Return the current performance trace id.
                pTraceId id() const { return _pTraceId; }

                // Stop the current performance trace  (if set) and start a new one with the provided parameters.
                void stopAndStart(const SentryHandler::PTraceName &pTraceName, int syncDbId = -1,
                                  bool manualStopExpected = false) {
                    stop(false);
                    _pTraceId = SentryHandler::instance()->startPTrace(pTraceName, syncDbId);
                    _manualStopExpected = manualStopExpected;
                }

                // Stop the current performance trace  (if set).
                void stop(bool aborted = false) noexcept {
                    if (_pTraceId) {
                        SentryHandler::instance()->stopPTrace(_pTraceId, aborted);
                        _pTraceId = 0;
                    }
                }

            private:
                pTraceId _pTraceId{0};
                bool _manualStopExpected{false};
                ScopedPTrace &operator=(ScopedPTrace &&) = delete;
        };

    public:
        virtual ~SentryHandler();
        static std::shared_ptr<SentryHandler> instance();
        static void init(KDC::AppType appType, int breadCrumbsSize = 100);
        void setAuthenticatedUser(const SentryUser &user);
        void setGlobalConfidentialityLevel(SentryConfidentialityLevel level);

        // Capture an event
        /*   If the same event has been captured more than 10 times in the last 10 minutes, it will be flagged as a rate
         * limited event. If a rate limited event is not seen for 10 minutes, it will be unflagged.
         *
         *   Rate limited events:
         *   - Will see their level escalated to Error
         *   - Will be upload to Sentry only if the last upload is older than 10 minutes else it will just increment the
         * capture count.
         *   - The event sent to Sentry will have a message indicating that the event has been rate limited.(see:
         *      void SentryHandler::escalateSentryEvent(SentryEvent &event))
         */
        void captureMessage(SentryLevel level, const std::string &title, std::string message,
                            const SentryUser &user = SentryUser());

        // Performances monitoring
        pTraceId startPTrace(SentryHandler::PTraceName pTraceName, int syncDbId = -1);
        void stopPTrace(SentryHandler::PTraceName pTraceName, int syncDbId = -1, bool aborted = false);
        void stopPTrace(const pTraceId &pTraceId, bool aborted = false);


        // Debugging
        inline static AppType appType() { return _appType; }
        inline static bool debugCrashCallback() { return _debugCrashCallback; }
        inline static bool debugBeforeSendCallback() { return _debugBeforeSendCallback; }

        // Print an event description into a file (for debugging)
        static void writeEvent(const std::string &eventStr, bool crash) noexcept;

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

        std::recursive_mutex _mutex;
        static std::shared_ptr<SentryHandler> _instance;
        bool _isSentryActivated = false;

        // The user that will be used to send genral events to Sentry. This variable can be updated with
        // `setAuthenticatedUser(...)`
        SentryUser _authenticatedUser;

        SentryConfidentialityLevel _globalConfidentialityLevel = SentryConfidentialityLevel::Anonymous; // Default value
        SentryConfidentialityLevel _lastConfidentialityLevel = SentryConfidentialityLevel::None;

        // Convert a `SentryUser` structure to a `sentry_value_t` that can safely be passed to
        // `sentry_set_user(sentry_value_t)`
        sentry_value_t toSentryValue(const SentryUser &user) const;

        struct StringHash {
                using is_transparent = void; // Enables heterogeneous operations.
                std::size_t operator()(std::string_view sv) const {
                    std::hash<std::string_view> hasher;
                    return hasher(sv);
                }
        };

        // SentryEvent is a structure that represents an event that has been send to sentry. It allow us to keep track of
        // the sended event and block any sentry flood from a single user.
        struct SentryEvent {
                using time_point = std::chrono::system_clock::time_point;

                SentryEvent(const std::string &title, const std::string &message, SentryLevel level,
                            SentryConfidentialityLevel userType, const SentryUser &user);
                std::string getStr() const { return title + message + static_cast<char>(level) + userId; }
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
         *  The event passed as parameter will be updated if it is rate limite is reached (level and message).
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
         * - If `user` parmater is not provided, the effective user will be set to Anonymous.
         * - If authentication type is `authenticated` and `user` parmater is provided, the user parameter will be used.
         */
        void updateEffectiveSentryUser(const SentryUser &user = SentryUser());

        // The events that have been recently captured, used to prevent flood from a single user.
        std::unordered_map<std::string, SentryEvent, StringHash, std::equal_to<>> _events;

        // Number of capture before rate limiting an event
        unsigned int _sentryMaxCaptureCountBeforeRateLimit = 10;

        // Min. interval between two uploads of a rate limited event (seconds)
        int _sentryMinUploadIntervaOnRateLimit = 60;

        // A PerformanceTrace allow to track the duration of several part of the code.
        // https://docs.sentry.io/product/performance/
        struct PerformanceTrace {
                // PerformanceTrace is a structure that represents a sentry_transaction_t or a sentry_span_t that can be sent to
                // Sentry.

                explicit PerformanceTrace(pTraceId pTraceId);
                void setTransaction(sentry_transaction_t *tx) { // A transaction cannot have a parent.
                    assert(!_span);
                    _tx = tx;
                }
                void setSpan(sentry_span_t *span) { // A Span behave as a transaction, but it needs to have a parent
                                                    // (which can be either a transaction or another span).
                    assert(!_span);
                    _span = span;
                }
                bool isSpan() const { return _span; }
                sentry_span_t *span() const {
                    assert(_span);
                    return _span;
                }
                sentry_transaction_t *transaction() const {
                    assert(_tx);
                    return _tx;
                }

                void addChild(pTraceId childId) { childIds.push_back(childId); }
                void removeChild(pTraceId childId) { childIds.remove(childId); }
                const std::list<pTraceId> &children() const { return childIds; }

                const pTraceId &id() const { return _pTraceId; }

            private:
                pTraceId _pTraceId;
                std::list<pTraceId> childIds;
                sentry_transaction_t *_tx{nullptr};
                sentry_span_t *_span{nullptr};
        };

        pTraceId _pTraceIdCounter = 0;
        std::map<pTraceId, PerformanceTrace> _performanceTraces;

        pTraceId startTransaction(const std::string &name, const std::string &description);
        pTraceId startSpan(const std::string &name, const std::string &description, const pTraceId &parentId);

        struct PTraceInfo {
                explicit PTraceInfo(SentryHandler::PTraceName transactionIdentifier);
                PTraceName _pTraceName = SentryHandler::PTraceName::None;
                PTraceName _parentPTraceName = SentryHandler::PTraceName::None;
                std::string _pTraceTitle;
                std::string _pTraceDescription;
        };
        std::map<int /*syncDbId*/, std::map<SentryHandler::PTraceName, pTraceId>> _pTraceNameToPTraceIdMap;

        // Debug
        static KDC::AppType _appType;
        static bool _debugCrashCallback;
        static bool _debugBeforeSendCallback;
};
} // namespace KDC
