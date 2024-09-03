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

#include "utility/types.h"
#include "sentryuser.h"

namespace KDC {

enum class SentryLevel {  // Not defined in types.h as we don't want to include sentry.h in types.h
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
        enum class SentryProject { Server, Client, Desactivated };  // Only used for initialization, don't need to be in types.h

        ~SentryHandler();
        static std::shared_ptr<SentryHandler> instance();
        static void init(SentryProject project, int breadCrumbsSize = 100);
        void setAuthenticatedUser(const SentryUser &user);
        void setGlobalConfidentialityLevel(SentryConfidentialityLevel level);
        void captureMessage(SentryLevel level, const std::string &title, std::string message,
                            const SentryUser &user = SentryUser());

    private:
        static std::shared_ptr<SentryHandler> _instance;
        SentryHandler() = default;
        SentryHandler(const SentryHandler &) = delete;
        SentryHandler &operator=(const SentryHandler &) = delete;
        SentryHandler(SentryHandler &&) = delete;
        SentryHandler &operator=(SentryHandler &&) = delete;

        sentry_value_t toSentryValue(const SentryUser &user);

        struct SentryEvent {
                using time_point = std::chrono::system_clock::time_point;

                SentryEvent(const std::string &title, const std::string &message, SentryLevel level,
                            SentryConfidentialityLevel userType, const SentryUser &user);
                std::string getStr() const { return title + message + (char)toInt(level) + userId; };
                std::string title;
                std::string message;
                SentryLevel level;
                SentryConfidentialityLevel confidentialityLevel = SentryConfidentialityLevel::Anonymous;
                std::string userId;
                time_point lastCapture;
                time_point lastUpload;
                unsigned int captureCount = 0;
        };

        std::mutex _mutex;
        bool _isSentryActivated = false;
        SentryUser _authenticatedUser;
        struct StringHash {
                using is_transparent = void;  // Enables heterogeneous operations.
                std::size_t operator()(std::string_view sv) const {
                    std::hash<std::string_view> hasher;
                    return hasher(sv);
                }
        };

        std::unordered_map<std::string, SentryEvent, StringHash, std::equal_to<>> _events;
        SentryConfidentialityLevel _globalConfidentialityLevel = SentryConfidentialityLevel::Anonymous;  // Default value
        SentryConfidentialityLevel _lastConfidentialityLevel = SentryConfidentialityLevel::None;
};
}  // namespace KDC