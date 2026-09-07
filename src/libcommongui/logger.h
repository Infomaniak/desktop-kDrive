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
#include "libcommongui/utility/utility.h"

#include <QObject>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QTimer>

#include <atomic>
#include <chrono>

namespace KDC {

struct GuiLog {
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

        void log(GuiLog log);
        void doLog(const QString &log, bool flush = false);

        static void kdriveLog(const QString &message);

        static Logger *instance();
        static void installMessagePattern();
        static void installEarlyMessageHandler();
        static void setSentryBreadcrumbsEnabled(bool enabled);
        static bool sentryBreadcrumbsEnabled() { return _sentryBreadcrumbsEnabled.load(std::memory_order_relaxed); }

        void postNotification(const QString &title, const QString &message);

        QFileDevice::FileError setLogFile(const QString &name, QIODeviceBase::OpenMode mode = QIODevice::WriteOnly);
        void setLogExpire(std::chrono::days expire);
        void setLogDir(const QString &dir);

        bool qtLoggingRulesEnabled() const { return _qtLoggingRulesEnabled; }
        void setQtLoggingRulesEnabled(bool enabled);

        /** Returns where the automatic logdir would be */
        QString logDirectoryPath() const { return _logDirectoryPath; }

        /** Sets up default dir log setup. */
        void setupLogDir();

        /** For switching off via logwindow */
        void disableLog();

        LogLevel minLogLevel() const;
        void setMinLogLevel(LogLevel level);

        static bool compressSingleLog(const QString &sourceName, const QString &targetName);

    signals:
        void showNotification(const QString &, const QString &);

    public slots:
        void enterNextLogFile();

    private slots:
        void slotWatchLogSize();

    private:
        explicit Logger(QObject *parent = nullptr);
        ~Logger() override;
        QFile _logFile;
        std::chrono::days _logExpire{0};
        bool _qtLoggingRulesEnabled{false};
        QScopedPointer<QTextStream> _logstream;
        mutable QMutex _mutex;
        QString _logDirectoryPath;
        bool _logEnabled = false;
        std::atomic<LogLevel> _minLogLevel = LogLevel::Debug;
        QTimer _watchLogSizeTimer;
        inline static std::atomic_bool _sentryBreadcrumbsEnabled{false};
};

} // namespace KDC
