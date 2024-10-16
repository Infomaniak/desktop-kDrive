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

#include "log.h"
#include "customrollingfileappender.h"
#include "libcommon/utility/utility.h"

#include <log4cplus/initializer.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/loggingmacros.h>

#include <codecvt>

namespace KDC {

const std::wstring Log::instanceName = L"Main";
const std::wstring Log::rfName = L"RollingFileAppender";
const std::wstring Log::rfPattern = L"%D{%Y-%m-%d %H:%M:%S:%q} [%-0.-1p] (%t) %b:%L - %m%n";
const int Log::rfMaxBackupIdx = 4; // Max number of backup files

std::shared_ptr<Log> Log::_instance = nullptr;

Log::~Log() {
    log4cplus::Logger::shutdown();
}

std::shared_ptr<Log> Log::instance(const log4cplus::tstring &filePath) noexcept {
    if (_instance == nullptr) {
        if (filePath.empty()) {
            assert(false);
            return nullptr;
        }

        try {
            _instance = std::shared_ptr<Log>(new Log(filePath));
            _instance->checkForExpiredFiles();
        } catch (...) {
            assert(false);
            return nullptr;
        }
    }

    return _instance;
}

bool Log::configure(bool useLog, LogLevel logLevel, bool purgeOldLogs) {
    if (useLog) {
        // Set log level
        switch (logLevel) {
            case LogLevel::Debug:
                _logger.setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
                break;
            case LogLevel::Info:
                _logger.setLogLevel(log4cplus::INFO_LOG_LEVEL);
                break;
            case LogLevel::Warning:
                _logger.setLogLevel(log4cplus::WARN_LOG_LEVEL);
                break;
            case LogLevel::Error:
                _logger.setLogLevel(log4cplus::ERROR_LOG_LEVEL);
                break;
            case LogLevel::Fatal:
                _logger.setLogLevel(log4cplus::FATAL_LOG_LEVEL);
                break;
        }
    } else {
        _logger.setLogLevel(log4cplus::OFF_LOG_LEVEL);
    }

    // Set purge rate
    log4cplus::SharedAppenderPtr rfAppenderPtr = _logger.getAppender(Log::rfName);
    static_cast<CustomRollingFileAppender *>(rfAppenderPtr.get())
            ->setExpire(purgeOldLogs ? CommonUtility::logsPurgeRate * 24 * 3600 : 0);

    return true;
}

Log::Log(const log4cplus::tstring &filePath) : _filePath(filePath) {
    // Instantiate an appender object
    CustomRollingFileAppender *rfAppender =
            new CustomRollingFileAppender(filePath, CommonUtility::logMaxSize, Log::rfMaxBackupIdx, true, true);

    // Unicode management
    std::locale loc(std::locale(), new std::codecvt_utf8<wchar_t>);
    rfAppender->imbue(loc);

    log4cplus::SharedAppenderPtr appender(std::move(rfAppender));
    appender->setName(Log::rfName);

    // Instantiate a layout object && attach the layout object to the appender
    appender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(Log::rfPattern)));

    // Instantiate a logger object
    _logger = log4cplus::Logger::getInstance(Log::instanceName);
    _logger.setLogLevel(log4cplus::TRACE_LOG_LEVEL);

    // Attach the appender object to the logger
    _logger.addAppender(appender);

    LOG_INFO(_logger, "Logger initialization done");
}

SyncPath Log::getLogFilePath() const {
    return _filePath;
}

void Log::checkForExpiredFiles() {
    auto *customRollingFileAppender = static_cast<CustomRollingFileAppender *>(_logger.getAppender(Log::rfName).get());
    if (customRollingFileAppender) {
        customRollingFileAppender->checkForExpiredFiles();
    } else {
        assert(false);
    }
}

} // namespace KDC
