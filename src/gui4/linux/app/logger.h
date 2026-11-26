/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "libcommon/utility/cstypes.h"

#include <QFile>
#include <QScopedPointer>
#include <QTextStream>
#include <QMutex>

#include <atomic>
#include <chrono>

namespace KDC {

class Logger {
    public:
        bool isLoggingToFile() const;

        static Logger *instance();
        static void installEarlyMessageHandler();

        void setLogExpire(std::chrono::days expire);
        /** Removes this logger's log files older than the configured expiration. Does nothing when expiration is 0. */
        void purgeExpiredLogFiles() const;

        /** Returns where the automatic logdir would be */
        QString logDirectoryPath() const { return _logDirectoryPath; }

        void enableLog();

        void disableLog();

        bool setMinLogLevel(LogLevel level);

    private:
        Logger();
        static void handleMessage(QtMsgType type, const QMessageLogContext &context, const QString &message);
        void enterNextLogFile();
        void setLogFile(const QString &name);
        ~Logger();
        static QString logAppName();
        QFile _logFile;
        std::chrono::days _logExpire{0};
        QScopedPointer<QTextStream> _logstream;
        mutable QMutex _mutex;
        QString _logDirectoryPath;
        std::atomic<LogLevel> _minLogLevel{LogLevel::Debug};
};

} // namespace KDC
