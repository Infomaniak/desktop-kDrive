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

#include "libcommongui/utility/utility.h"

#include <QObject>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QTimer>

#include <chrono>

namespace KDC {

struct Log {
        QDateTime timeStamp;
        QString message;
};

/**
 * @brief The Logger class
 * @ingroup libsync
 */
class Logger : public QObject {
        Q_OBJECT

    public:
        bool isNoop() const;
        bool isLoggingToFile() const;

        void log(Log log);
        void doLog(const QString &log);

        static void kdriveLog(const QString &message);

        static Logger *instance();

        void postNotification(const QString &title, const QString &message);

        void setLogFile(const QString &name);
        void setLogExpire(std::chrono::hours expire);
        void setLogDir(const QString &dir);
        void setLogFlush(bool flush);

        bool logDebug() const { return _logDebug; }
        void setLogDebug(bool debug);

        /** Returns where the automatic logdir would be */
        QString temporaryFolderLogDirPath() const;

        /** Sets up default dir log setup.
         *
         * logdir: a temporary folder
         * logdebug: true
         *
         */
        void setupTemporaryFolderLogDir();

        /** For switching off via logwindow */
        void disableTemporaryFolderLogDir();

        int minLogLevel() const;
        void setMinLogLevel(int level);

        bool compressSingleLog(const QString &sourceName, const QString &targetName);

        void setIsCLientLog(bool newIsCLientLog);

    signals:
        void logWindowLog(const QString &);

        void showNotification(const QString &, const QString &);
        void logTooBig();

    public slots:
        void enterNextLogFile();

    private slots:
        void slotWatchLogSize();

    private:
        Logger(QObject *parent = 0);
        ~Logger();
        bool _showTime;
        QFile _logFile;
        bool _doFileFlush;
        std::chrono::hours _logExpire;
        bool _logDebug;
        QScopedPointer<QTextStream> _logstream;
        mutable QMutex _mutex;
        QString _logDirectory;
        bool _temporaryFolderLogDir = false;
        int _minLogLevel;
        QTimer _watchLogSizeTimer;
        bool _isCLientLog = false;
};

} // namespace KDC
