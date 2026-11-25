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

#include "logger.h"
#include "config.h"
#include "libcommon/utility/utility.h"

#include <sentry.h>

#include <QDir>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QStringList>
#include <QDateTime>

#include <atomic>
#include <cstdint>
#include <iostream>

constexpr char logMessagePattern[] =
        "%{time yyyy-MM-dd hh:mm:ss:zzz} "
        "[%{if-debug}D%{endif}%{if-info}I%{endif}%{if-warning}W%{endif}%{if-critical}C%{endif}%{if-fatal}F%{endif}] "
        "(%{threadid}) %{file}:%{line} - %{message}";

namespace KDC {

static LogLevel logLevelForMessageType(const QtMsgType type) noexcept {
    switch (type) {
        case QtDebugMsg:
            return LogLevel::Debug;
        case QtInfoMsg:
            return LogLevel::Info;
        case QtWarningMsg:
            return LogLevel::Warning;
        case QtCriticalMsg: // In Qt's implem, QtCriticalMsg == QtSystemMsg
            return LogLevel::Error;
        case QtFatalMsg:
            return LogLevel::Fatal;
    }

    Q_UNREACHABLE();
}

static const char *sentryLevelForMessageType(const QtMsgType type) noexcept {
    switch (type) {
        case QtDebugMsg:
            return "debug";
        case QtInfoMsg:
            return "info";
        case QtWarningMsg:
            return "warning";
        case QtCriticalMsg:
            return "error";
        case QtFatalMsg:
            return "fatal";
    }

    Q_UNREACHABLE();
}

static QString formatLogMessageWithShortFile(const QtMsgType type, const QMessageLogContext &ctx, const QString &message) {
    SyncName fileName;
    if (ctx.file != nullptr) {
        const SyncPath filePath(ctx.file);
        fileName = filePath.filename();
    }
    const QMessageLogContext ctxNew(fileName.c_str(), ctx.line, ctx.function, ctx.category);
    return qFormatLogMessage(type, ctxNew, message);
}

static QString formatSentryBreadcrumb(const QMessageLogContext &ctx, const QString &message) {
    if (ctx.file == nullptr) {
        return message;
    }

    const SyncName fileName = SyncPath(ctx.file).filename();
    const QString fileNameString = QString::fromStdString(fileName);
    return QStringLiteral("%1:%2 - %3").arg(fileNameString).arg(ctx.line).arg(message);
}

static void addSentryBreadcrumb(const QtMsgType type, const QMessageLogContext &ctx, const QString &message) {
    const std::string breadcrumbMessage = formatSentryBreadcrumb(ctx, message).toStdString();
    const sentry_value_t breadcrumb = sentry_value_new_breadcrumb("default", breadcrumbMessage.c_str());
    (void) sentry_value_set_by_key(breadcrumb, "level", sentry_value_new_string(sentryLevelForMessageType(type)));

    if (ctx.category != nullptr) {
        static const auto guiV4Prefix = QStringLiteral("gui.v4.");
        QString category = QString::fromUtf8(ctx.category);
        if (category.startsWith(guiV4Prefix)) {
            (void) category.remove(0, guiV4Prefix.size());
        }
        const std::string breadcrumbCategory = category.toStdString();
        (void) sentry_value_set_by_key(breadcrumb, "category", sentry_value_new_string(breadcrumbCategory.c_str()));
    }

    sentry_add_breadcrumb(breadcrumb);
}

static void earlyLogCatcher(const QtMsgType type, const QMessageLogContext &ctx, const QString &message) {
    const QString formattedMessage = formatLogMessageWithShortFile(type, ctx, message);
    addSentryBreadcrumb(type, ctx, message);
    if (CommonUtility::logToConsoleEnabled()) {
        std::cerr << qPrintable(formattedMessage) << '\n';
    }
}

void Logger::handleMessage(const QtMsgType type, const QMessageLogContext &ctx, const QString &message) {
    addSentryBreadcrumb(type, ctx, message);
    auto *const logger = instance();
    const auto level = logLevelForMessageType(type);
    if (level < logger->_minLogLevel.load(std::memory_order_relaxed)) {
        return;
    }

    const QString formatted = formatLogMessageWithShortFile(type, ctx, message);
    {
        QMutexLocker lock(&logger->_mutex);
        if (!logger->_logstream) {
            if (level >= LogLevel::Error) {
                std::cerr << qPrintable(formatted) << '\n';
            }
            return;
        }
        (*logger->_logstream) << formatted << Qt::endl;
    }
#ifndef NDEBUG
    if (CommonUtility::logToConsoleEnabled()) {
        std::cerr << qPrintable(formatted) << '\n';
    }
#endif
}

Logger *Logger::instance() {
    static Logger log;
    return &log;
}

void Logger::installEarlyMessageHandler() {
    qSetMessagePattern(QString::fromLatin1(logMessagePattern));
#ifndef NO_MSG_HANDLER
    (void) qInstallMessageHandler(earlyLogCatcher);
#else
    Q_UNUSED(earlyLogCatcher)
#endif
}

Logger::Logger() {
#ifndef NO_MSG_HANDLER
    (void) qInstallMessageHandler(handleMessage);
#else
    Q_UNUSED(handleMessage)
#endif
}

Logger::~Logger() {
#ifndef NO_MSG_HANDLER
    (void) qInstallMessageHandler(nullptr);
#endif
}

bool Logger::setMinLogLevel(const LogLevel level) {
    return _minLogLevel.exchange(level, std::memory_order_relaxed) != level;
}

bool Logger::isLoggingToFile() const {
    QMutexLocker lock(&_mutex);
    return !_logstream.isNull();
}

void Logger::setLogFile(const QString &name) {
    const QMutexLocker locker(&_mutex);
    if (_logstream) {
        _logstream.reset(0);
        _logFile.close();
    }

    if (name.isEmpty()) {
        return;
    }

    _logFile.setFileName(name);
    if (!_logFile.open(QIODevice::WriteOnly)) {
        std::cerr << "Cannot open log file " << qPrintable(name) << ": " << qPrintable(_logFile.errorString()) << '\n';
        return;
    }

    _logstream.reset(new QTextStream(&_logFile));
}

void Logger::setLogExpire(const std::chrono::days expire) {
    _logExpire = expire;
}

void Logger::purgeExpiredLogFiles() const {
    if (_logExpire.count() <= 0) {
        return;
    }

    // File logging may be disabled, which clears the log directory: fall back to the default one so the cleanup still applies.
    QString logDirectoryPath = _logDirectoryPath;
    if (logDirectoryPath.isEmpty()) {
        SyncPath defaultLogDirectoryPath;
        if (!CommonUtility::logDirectoryPath(defaultLogDirectoryPath)) {
            return;
        }
        logDirectoryPath = Path2QStr(defaultLogDirectoryPath);
    }

    QDir dir(logDirectoryPath);
    const QDateTime now = QDateTime::currentDateTime();
    for (const QStringList files = dir.entryList(QStringList(QString("*%1.log.*").arg(logAppName())), QDir::Files, QDir::Name);
         const QString &fileName: files) {
        if (const QFileInfo fileInfo(dir.absoluteFilePath(fileName)); fileInfo.lastModified().addDays(_logExpire.count()) < now) {
            (void) dir.remove(fileName);
        }
    }
}

QString Logger::logAppName() {
    return QString("%1_client").arg(APPLICATION_NAME);
}

void Logger::enableLog() {
    QLoggingCategory::setFilterRules(QStringLiteral("*=false\ngui.v4.*=true"));
    SyncPath path;
    (void) CommonUtility::logDirectoryPath(path);
    const QString logDirPath = Path2QStr(path);
    if (logDirPath.isEmpty()) return;
    if (!QDir().mkpath(logDirPath)) return;
    _logDirectoryPath = logDirPath;
    enterNextLogFile();
}

void Logger::disableLog() {
    enterNextLogFile();
    _logDirectoryPath.clear();
    QLoggingCategory::setFilterRules({});
    setLogFile(QString());
}

void Logger::enterNextLogFile() {
    if (_logDirectoryPath.isEmpty()) return;

    QDir dir(_logDirectoryPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Tentative new log name, will be adjusted if one like this already exists
    const QDateTime now = QDateTime::currentDateTime();
    const QString appName = logAppName();
    QString newLogName = now.toString("yyyyMMdd_HHmm") + QString("_%1.log").arg(appName);

    // Expire old log files and deal with conflicts
    purgeExpiredLogFiles();
    const QStringList files = dir.entryList(QStringList(QString("*%1.log.*").arg(appName)), QDir::Files, QDir::Name);
    QString rxPattern(QString(R"(.*%1\.log\.(\d+).*)").arg(appName));
    rxPattern = QRegularExpression::anchoredPattern(rxPattern);

    QString unzippedPattern(QString(R"(.*%1\.log\.\d$)").arg(appName));
    unzippedPattern = QRegularExpression::anchoredPattern(unzippedPattern);

    int32_t maxNumber = -1;
    QStringList unzippedFiles;
    foreach (const QString &s, files) {
        QRegularExpressionMatch rxMatch = QRegularExpression(rxPattern).match(s);
        if (s.startsWith(newLogName) && rxMatch.hasMatch()) {
            maxNumber = qMax(maxNumber, rxMatch.captured(1).toInt());
        } else if (QRegularExpression(unzippedPattern).match(s).hasMatch()) {
            unzippedFiles.append(dir.absoluteFilePath(s));
        }
    }
    newLogName.append("." + QString::number(maxNumber + 1));

    setLogFile(dir.filePath(newLogName));

    // Compress the previous log file. On a restart this can be the most recent
    // log file.
    for (const QString &logToCompress: unzippedFiles) {
        if (!logToCompress.isEmpty()) {
            QString compressedName = logToCompress + ".gz";
            if (QFile::exists(compressedName)) {
                QFile::remove(compressedName);
            }

            if (KDC::CommonUtility::compressFile(logToCompress, compressedName)) {
                QFile::remove(logToCompress);
            } else {
                QFile::remove(compressedName);
            }
        }
    }
}

} // namespace KDC
