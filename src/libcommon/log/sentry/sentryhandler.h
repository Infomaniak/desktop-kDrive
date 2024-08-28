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
#include <sentry.h>
#include <string>
#include "utility/types.h"
#include <mutex>

namespace KDC {

enum class SentryLevel {  // Not defined in types.h as we don't want to include sentry.h in types.h
    Debug = SENTRY_LEVEL_DEBUG,
    Info = SENTRY_LEVEL_INFO,
    Warning = SENTRY_LEVEL_WARNING,
    Error = SENTRY_LEVEL_ERROR,
    Fatal = SENTRY_LEVEL_FATAL
};

enum class SentryProject { Server, Client };

class SentryHandler {
    public:
        ~SentryHandler();

        enum class SentryUserType {
            Anonymous,  // The sentry will not be able to identify the user (no ip, no email, no username, ...)
            Unknown,    // The sentry will be able to uniquely identify the user with his IP address but nothing else.
            Authenticated,  // The sentry will contains information about the last user connected to the application. (email,
                            // username, user id, ...)
            Specific        // The sentry will contains information about a specific user given in parameter.
        };

        struct SentryUser {
            public:
                SentryUser() = default;
                SentryUser(const std::string_view &email, const std::string_view &username, const std::string_view &userId)
                    : email(email), username(username), userId(userId) {}

                inline const std::string_view &getEmail() const { return email; };
                inline const std::string_view &getUsername() const { return username; };
                inline const std::string_view &getUserId() const { return userId; };

            private:
                std::string email = "Not set";
                std::string username = "Not set";
                std::string userId = "Not set";
        };

        static std::shared_ptr<SentryHandler> instance();
        static void init(SentryProject project, int breadCrumbsSize = 100);
        void setAuthenticatedUser(const SentryUser &user);
        void captureMessage(SentryLevel level, const std::string &title, const std::string &message,
                            SentryUserType userType = SentryUserType::Authenticated,
                            const SentryUser &user = SentryUser() /*Only for UserType::Specific*/);

    private:
        static std::shared_ptr<SentryHandler> _instance;
        SentryHandler() = default;
        SentryHandler(const SentryHandler &) = delete;
        SentryHandler &operator=(const SentryHandler &) = delete;
        SentryHandler(SentryHandler &&) = delete;
        SentryHandler &operator=(SentryHandler &&) = delete;

        sentry_value_t toSentryValue(const SentryUser &user);

    private:
        std::mutex _mutex;
        sentry_value_t _authenticatedUser;
};
}  // namespace KDC