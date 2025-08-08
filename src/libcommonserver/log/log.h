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

#include "libcommonserver/commonserverlib.h"

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

#include "libcommon/log/sentry/handler.h"
#include "libcommon/log/customlogstreams.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"

namespace KDC {

#ifdef NDEBUG
#define LOG_DEBUG(logger, logEvent)                                                               \
    {                                                                                             \
        CustomLogStream customLogStream_;                                                         \
        customLogStream_ << logEvent;                                                             \
        const auto &customLogStreamStr_ = customLogStream_.str();                                 \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, customLogStreamStr_.c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("debug"));                \
        sentry_add_breadcrumb(crumb);                                                             \
        LOG4CPLUS_DEBUG(logger, customLogStreamStr_.c_str());                                     \
    }

#define LOGW_DEBUG(logger, logEvent)                                                                                    \
    {                                                                                                                   \
        CustomLogWStream customLogWStream_;                                                                             \
        customLogWStream_ << logEvent;                                                                                  \
        const auto &customLogWStreamStr_ = customLogWStream_.str();                                                     \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, CommonUtility::ws2s(customLogWStreamStr_).c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("debug"));                                      \
        sentry_add_breadcrumb(crumb);                                                                                   \
        LOG4CPLUS_DEBUG(logger, customLogWStreamStr_.c_str());                                                          \
    }

#define LOG_INFO(logger, logEvent)                                                                \
    {                                                                                             \
        CustomLogStream customLogStream_;                                                         \
        customLogStream_ << logEvent;                                                             \
        const auto customLogStreamStr_ = customLogStream_.str();                                  \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, customLogStreamStr_.c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("info"));                 \
        sentry_add_breadcrumb(crumb);                                                             \
        LOG4CPLUS_INFO(logger, customLogStreamStr_.c_str());                                      \
    }

#define LOGW_INFO(logger, logEvent)                                                                                     \
    {                                                                                                                   \
        CustomLogWStream customLogWStream_;                                                                             \
        customLogWStream_ << logEvent;                                                                                  \
        const auto &customLogWStreamStr_ = customLogWStream_.str();                                                     \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, CommonUtility::ws2s(customLogWStreamStr_).c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("info"));                                       \
        sentry_add_breadcrumb(crumb);                                                                                   \
        LOG4CPLUS_INFO(logger, customLogWStreamStr_.c_str());                                                           \
    }

#define LOG_WARN(logger, logEvent)                                                                \
    {                                                                                             \
        CustomLogStream customLogStream_;                                                         \
        customLogStream_ << logEvent;                                                             \
        const auto customLogStreamStr_ = customLogStream_.str();                                  \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, customLogStreamStr_.c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("warning"));              \
        sentry_add_breadcrumb(crumb);                                                             \
        LOG4CPLUS_WARN(logger, customLogStreamStr_.c_str());                                      \
    }


#define LOGW_WARN(logger, logEvent)                                                                                     \
    {                                                                                                                   \
        CustomLogWStream customLogWStream_;                                                                             \
        customLogWStream_ << logEvent;                                                                                  \
        const auto &customLogWStreamStr_ = customLogWStream_.str();                                                     \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, CommonUtility::ws2s(customLogWStreamStr_).c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("warning"));                                    \
        sentry_add_breadcrumb(crumb);                                                                                   \
        LOG4CPLUS_WARN(logger, customLogWStreamStr_.c_str());                                                           \
    }

#define LOG_ERROR(logger, logEvent)                                                               \
    {                                                                                             \
        CustomLogStream customLogStream_;                                                         \
        customLogStream_ << logEvent;                                                             \
        const auto customLogStreamStr_ = customLogStream_.str();                                  \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, customLogStreamStr_.c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("error"));                \
        sentry_add_breadcrumb(crumb);                                                             \
        LOG4CPLUS_ERROR(logger, customLogStreamStr_.c_str());                                     \
    }

#define LOGW_ERROR(logger, logEvent)                                                                                    \
    {                                                                                                                   \
        CustomLogWStream customLogWStream_;                                                                             \
        customLogWStream_ << logEvent;                                                                                  \
        const auto &customLogWStreamStr_ = customLogWStream_.str();                                                     \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, CommonUtility::ws2s(customLogWStreamStr_).c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("error"));                                      \
        sentry_add_breadcrumb(crumb);                                                                                   \
        LOG4CPLUS_ERROR(logger, customLogWStreamStr_.c_str());                                                          \
    }

#define LOG_FATAL(logger, logEvent)                                                                    \
    {                                                                                                  \
        CustomLogStream customLogStream_;                                                              \
        customLogStream_ << logEvent;                                                                  \
        const auto customLogStreamStr_ = customLogStream_.str();                                       \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, customLogStreamStr_.c_str());      \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("fatal"));                     \
        sentry_add_breadcrumb(crumb);                                                                  \
        sentry::Handler::captureMessage(sentry::Level::Fatal, "Log fatal error", customLogStreamStr_); \
        LOG4CPLUS_FATAL(logger, customLogStreamStr_.c_str());                                          \
    }

#define LOGW_FATAL(logger, logEvent)                                                                                         \
    {                                                                                                                        \
        CustomLogWStream customLogWStream_;                                                                                  \
        customLogWStream_ << logEvent;                                                                                       \
        const auto &customLogWStreamStr_ = customLogWStream_.str();                                                          \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, CommonUtility::ws2s(customLogWStreamStr_).c_str());      \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("fatal"));                                           \
        sentry_add_breadcrumb(crumb);                                                                                        \
        sentry::Handler::captureMessage(sentry::Level::Fatal, "Log fatal error", CommonUtility::ws2s(customLogWStreamStr_)); \
        LOG4CPLUS_FATAL(logger, customLogWStreamStr_.c_str());                                                               \
    }
#else
#define LOG_DEBUG(logger, logEvent)                              \
    {                                                            \
        CustomLogStream customLogStream_;                        \
        customLogStream_ << logEvent;                            \
        LOG4CPLUS_DEBUG(logger, customLogStream_.str().c_str()); \
    }

#define LOGW_DEBUG(logger, logEvent)                              \
    {                                                             \
        CustomLogWStream customLogWstream_;                       \
        customLogWstream_ << logEvent;                            \
        LOG4CPLUS_DEBUG(logger, customLogWstream_.str().c_str()); \
    }

#define LOG_INFO(logger, logEvent)                              \
    {                                                           \
        CustomLogStream customLogStream_;                       \
        customLogStream_ << logEvent;                           \
        LOG4CPLUS_INFO(logger, customLogStream_.str().c_str()); \
    }

#define LOGW_INFO(logger, logEvent)                              \
    {                                                            \
        CustomLogWStream customLogWstream_;                      \
        customLogWstream_ << logEvent;                           \
        LOG4CPLUS_INFO(logger, customLogWstream_.str().c_str()); \
    }

#define LOG_WARN(logger, logEvent)                              \
    {                                                           \
        CustomLogStream customLogStream_;                       \
        customLogStream_ << logEvent;                           \
        LOG4CPLUS_WARN(logger, customLogStream_.str().c_str()); \
    }

#define LOGW_WARN(logger, logEvent)                              \
    {                                                            \
        CustomLogWStream customLogWstream_;                      \
        customLogWstream_ << logEvent;                           \
        LOG4CPLUS_WARN(logger, customLogWstream_.str().c_str()); \
    }

#define LOG_ERROR(logger, logEvent)                              \
    {                                                            \
        CustomLogStream customLogStream_;                        \
        customLogStream_ << logEvent;                            \
        LOG4CPLUS_ERROR(logger, customLogStream_.str().c_str()); \
    }

#define LOGW_ERROR(logger, logEvent)                              \
    {                                                             \
        CustomLogWStream customLogWstream_;                       \
        customLogWstream_ << logEvent;                            \
        LOG4CPLUS_ERROR(logger, customLogWstream_.str().c_str()); \
    }

#define LOG_FATAL(logger, logEvent)                              \
    {                                                            \
        CustomLogStream customLogStream_;                        \
        customLogStream_ << logEvent;                            \
        LOG4CPLUS_FATAL(logger, customLogStream_.str().c_str()); \
    }

#define LOGW_FATAL(logger, logEvent)                              \
    {                                                             \
        CustomLogWStream customLogWstream_;                       \
        customLogWstream_ << logEvent;                            \
        LOG4CPLUS_FATAL(logger, customLogWstream_.str().c_str()); \
    }

#endif

class COMMONSERVER_EXPORT Log {
    public:
        ~Log();
        static std::shared_ptr<Log> instance(const log4cplus::tstring &filePath = log4cplus::tstring());

        Log(Log const &) = delete;
        void operator=(Log const &) = delete;

        static bool isSet() { return _instance != nullptr; }

        inline log4cplus::Logger getLogger() { return _logger; }
        bool configure(bool useLog, LogLevel logLevel, bool purgeOldLogs);

        /*! Returns the path of the log file.
         * \return The path of the log file.
         */
        SyncPath getLogFilePath() const;

        void checkForExpiredFiles();

        static const std::wstring instanceName;
        static const std::wstring rfName;
        static const std::wstring rfPattern;
        static const int rfMaxBackupIdx;

    private:
        friend class TestLog;
        friend class TestIo;
        explicit Log(const log4cplus::tstring &filePath);
        static std::shared_ptr<Log> _instance;
        log4cplus::Logger _logger;
        SyncPath _filePath;
};

} // namespace KDC
