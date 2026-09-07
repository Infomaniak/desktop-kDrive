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
#include <QThread>

#include <atomic>
#include <cstdint>
#include <iostream>


namespace KDC {

namespace {
static constexpr auto logSizeWatcherTimeout = std::chrono::minutes{1};

constexpr char logMessagePattern[] =
        "%{time yyyy-MM-dd hh:mm:ss:zzz} "
        "[%{if-debug}D%{endif}%{if-info}I%{endif}%{if-warning}W%{endif}%{if-critical}C%{endif}%{if-fatal}F%{endif}] "
        "(%{threadid}) %{file}:%{line} - %{message}";

struct LogMessageMetadata {
        QString fileName;
        QString category;
        bool hasCategory = false;
};
} // namespace

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

static LogMessageMetadata extractLogMessageMetadata(const QMessageLogContext &ctx) {
    LogMessageMetadata metadata;
    if (ctx.file != nullptr) {
        metadata.fileName = SyncName2QStr(SyncPath(ctx.file).filename());
    }

    if (ctx.category != nullptr) {
        metadata.category = QString::fromUtf8(ctx.category);
        metadata.hasCategory = true;
    }

    return metadata;
}

static QString formatLogMessageWithShortFile(const QtMsgType type, const QMessageLogContext &ctx, const QString &message,
                                             const LogMessageMetadata &metadata) {
    QString displayedMessage = message;
    if (metadata.hasCategory && metadata.category != QStringLiteral("gui.v4") &&
        !metadata.category.startsWith(QStringLiteral("gui.v4."))) {
        displayedMessage.prepend(QStringLiteral("[%1] ").arg(metadata.category));
    }

    const QByteArray fileName = metadata.fileName.toUtf8();
    const QMessageLogContext ctxNew(fileName.constData(), ctx.line, ctx.function, ctx.category);
    QString formattedMessage = qFormatLogMessage(type, ctxNew, displayedMessage);
    if (metadata.fileName.isEmpty() && ctx.line == 0) {
        const auto missingLocation = QStringLiteral(" :0 - ");
        const qsizetype missingLocationPosition = formattedMessage.indexOf(missingLocation);
        if (missingLocationPosition >= 0) {
            formattedMessage.replace(missingLocationPosition, missingLocation.size(), QStringLiteral(" "));
        }
    }

    return formattedMessage;
}

static QString formatSentryBreadcrumb(const QMessageLogContext &ctx, const QString &message, const LogMessageMetadata &metadata) {
    if (metadata.fileName.isEmpty()) {
        return message;
    }

    return QStringLiteral("%1:%2 - %3").arg(metadata.fileName).arg(ctx.line).arg(message);
}

static void addSentryBreadcrumb(const QtMsgType type, const QMessageLogContext &ctx, const QString &message,
                                const LogMessageMetadata &metadata) {
    const std::string breadcrumbMessage = formatSentryBreadcrumb(ctx, message, metadata).toStdString();
    const sentry_value_t breadcrumb = sentry_value_new_breadcrumb("default", breadcrumbMessage.c_str());
    (void) sentry_value_set_by_key(breadcrumb, "level", sentry_value_new_string(sentryLevelForMessageType(type)));

    if (metadata.hasCategory) {
        static const auto guiV4Prefix = QStringLiteral("gui.v4.");
        QString category = metadata.category;
        if (category.startsWith(guiV4Prefix)) {
            (void) category.remove(0, guiV4Prefix.size());
        }
        const std::string breadcrumbCategory = category.toStdString();
        (void) sentry_value_set_by_key(breadcrumb, "category", sentry_value_new_string(breadcrumbCategory.c_str()));
    }

    sentry_add_breadcrumb(breadcrumb);
}

static void earlyLogCatcher(const QtMsgType type, const QMessageLogContext &ctx, const QString &message) {
    const bool logToConsole = CommonUtility::logToConsoleEnabled();
    const bool addBreadcrumb = Logger::sentryBreadcrumbsEnabled();
    if (!logToConsole && !addBreadcrumb) {
        return;
    }

    const LogMessageMetadata metadata = extractLogMessageMetadata(ctx);
    if (addBreadcrumb) {
        addSentryBreadcrumb(type, ctx, message, metadata);
    }
    if (logToConsole) {
        const QString formattedMessage = formatLogMessageWithShortFile(type, ctx, message, metadata);
        std::cerr << qPrintable(formattedMessage) << '\n';
    }
}

static void kdriveLogCatcher(const QtMsgType type, const QMessageLogContext &ctx, const QString &message) {
    auto *const logger = Logger::instance();
    const bool addBreadcrumb = Logger::sentryBreadcrumbsEnabled();
    const bool logLocally = logLevelForMessageType(type) >= logger->minLogLevel();
    if (!addBreadcrumb && !logLocally) {
        return;
    }

    const LogMessageMetadata metadata = extractLogMessageMetadata(ctx);
    if (addBreadcrumb) {
        addSentryBreadcrumb(type, ctx, message, metadata);
    }

    if (!logLocally) return;

    const QString formattedMessage = formatLogMessageWithShortFile(type, ctx, message, metadata);
    if (!logger->isNoop()) {
        logger->doLog(formattedMessage, type == QtFatalMsg);
    } else if (type >= QtCriticalMsg) {
        std::cerr << qPrintable(formattedMessage) << '\n';
    }
}


Logger *Logger::instance() {
    static Logger log;
    return &log;
}

void Logger::installMessagePattern() {
    qSetMessagePattern(QString::fromLatin1(logMessagePattern));
}

void Logger::installEarlyMessageHandler() {
    installMessagePattern();
#ifndef NO_MSG_HANDLER
    qInstallMessageHandler(earlyLogCatcher);
#else
    Q_UNUSED(earlyLogCatcher)
#endif
}

void Logger::setSentryBreadcrumbsEnabled(const bool enabled) {
    _sentryBreadcrumbsEnabled.store(enabled, std::memory_order_relaxed);
}

Logger::Logger(QObject *parent) :
    QObject(parent) {
    installMessagePattern();
#ifndef NO_MSG_HANDLER
    qInstallMessageHandler(kdriveLogCatcher);
#else
    Q_UNUSED(kdriveLogCatcher)
#endif

    connect(&_watchLogSizeTimer, &QTimer::timeout, this, &Logger::slotWatchLogSize);
    _watchLogSizeTimer.start(logSizeWatcherTimeout);
}

Logger::~Logger() {
#ifndef NO_MSG_HANDLER
    qInstallMessageHandler(0);
#endif
    (void) setLogFile(QString());
}

LogLevel Logger::minLogLevel() const {
    return _minLogLevel.load(std::memory_order_relaxed);
}

void Logger::setMinLogLevel(const LogLevel level) {
    _minLogLevel.store(level, std::memory_order_relaxed);
}

void Logger::postNotification(const QString &title, const QString &message) {
    emit showNotification(title, message);
}

void Logger::log(GuiLog log) {
    QString msg = log.timeStamp.toString(QLatin1String("MM-dd hh:mm:ss:zzz")) + QLatin1Char(' ');
    msg += QString().asprintf("%p ", (void *) QThread::currentThread());
    msg += log.message;
    doLog(msg);
}

/**
 * Returns true if doLog does nothing and need not to be called
 */
bool Logger::isNoop() const {
    QMutexLocker lock(&_mutex);
    return !_logstream;
}

bool Logger::isLoggingToFile() const {
    QMutexLocker lock(&_mutex);
    return !_logstream.isNull();
}

void Logger::doLog(const QString &msg, const bool flush) {
    {
        QMutexLocker lock(&_mutex);
        if (_logstream) {
            (*_logstream) << msg << '\n';
            if (flush) _logstream->flush();
        }
    }
#ifndef NDEBUG
    if (CommonUtility::logToConsoleEnabled()) {
        std::cout << qPrintable(msg) << '\n';
        if (flush) std::cout.flush();
    }
#endif
}

void Logger::kdriveLog(const QString &message) {
    GuiLog log_;
    log_.timeStamp = QDateTime::currentDateTimeUtc();
    log_.message = message;

    instance()->log(log_);
}

QFileDevice::FileError Logger::setLogFile(const QString &name, const QIODeviceBase::OpenMode mode) {
    QMutexLocker locker(&_mutex);
    if (_logstream) {
        _logstream->flush();
        _logstream.reset(0);
        _logFile.close();
    }

    if (name.isEmpty()) {
        return QFileDevice::NoError;
    }

    bool openSucceeded = false;
    if (name == QLatin1String("-")) {
        openSucceeded = _logFile.open(stdout, QIODevice::WriteOnly);
    } else {
        _logFile.setFileName(name);
        openSucceeded = _logFile.open(mode);
    }

    if (!openSucceeded) {
        const QFileDevice::FileError error = _logFile.error();
        const QString errorString = _logFile.errorString();
        const bool fileAlreadyExists = mode.testFlag(QIODeviceBase::NewOnly) && QFile::exists(name);
        locker.unlock(); // Just in case postGuiMessage has a qDebug()
        if (!fileAlreadyExists) {
            std::cerr << "Unable to open log file '" << qPrintable(name) << "': " << qPrintable(errorString) << '\n';
            postNotification(tr("Error"), QString(tr("<nobr>File '%1'<br/>cannot be opened for writing.<br/><br/>"
                                                     "The log output can <b>not</b> be saved!</nobr>"))
                                                  .arg(name));
        }
        return error;
    }

    _logstream.reset(new QTextStream(&_logFile));
    return QFileDevice::NoError;
}

void Logger::setLogExpire(const std::chrono::days expire) {
    _logExpire = expire;
}

void Logger::setLogDir(const QString &dir) {
    _logDirectoryPath = dir;
}

void Logger::setQtLoggingRulesEnabled(const bool enabled) {
    QLoggingCategory::setFilterRules(enabled ? QStringLiteral("*=true\n"
                                                              "*.debug=false\n"
                                                              "gui.*.debug=true\n"
                                                              "qml.debug=true\n"
                                                              "qml.*.debug=true")
                                             : QString());
    _qtLoggingRulesEnabled = enabled;
}

void Logger::setupLogDir() {
    SyncPath path;
    (void) CommonUtility::logDirectoryPath(path);
    const QString logDirPath = Path2QStr(path);
    if (logDirPath.isEmpty()) return;
    if (!QDir().mkpath(logDirPath)) return;
    setQtLoggingRulesEnabled(true);
    setLogDir(logDirPath);
    _logEnabled = true;
}

void Logger::disableLog() {
    if (!_logEnabled) return;

    enterNextLogFile();
    setLogDir(QString());
    setQtLoggingRulesEnabled(false);
    (void) setLogFile(QString());
    _logEnabled = false;
}

void Logger::enterNextLogFile() {
    if (_logDirectoryPath.isEmpty()) return;

    QDir dir(_logDirectoryPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Tentative new log name, will be adjusted if one like this already exists
    const QDateTime now = QDateTime::currentDateTime();
    QString appName(APPLICATION_NAME);
    appName += QStringLiteral("_client");
    QString newLogName = now.toString("yyyyMMdd_HHmm") + QString("_%1.log").arg(appName);

    // Expire old log files and deal with conflicts
    const QStringList files = dir.entryList(QStringList(QString("*%1.log.*").arg(appName)), QDir::Files, QDir::Name);
    QString rxPattern(QString(R"(.*%1\.log\.(\d+).*)").arg(appName));
    rxPattern = QRegularExpression::anchoredPattern(rxPattern);

    QString unzippedPattern(QString(R"(.*%1\.log\.\d+$)").arg(appName));
    unzippedPattern = QRegularExpression::anchoredPattern(unzippedPattern);

    int32_t maxNumber = -1;
    QStringList unzippedFiles;
    foreach (const QString &s, files) {
        if (_logExpire.count() > 0) {
            const QFileInfo fileInfo(dir.absoluteFilePath(s));
            if (fileInfo.lastModified().addDays(_logExpire.count()) < now) {
                dir.remove(s);
            }
        }

        QRegularExpressionMatch rxMatch = QRegularExpression(rxPattern).match(s);
        if (s.startsWith(newLogName) && rxMatch.hasMatch()) {
            maxNumber = qMax(maxNumber, rxMatch.captured(1).toInt());
        } else if (QRegularExpression(unzippedPattern).match(s).hasMatch()) {
            unzippedFiles.append(dir.absoluteFilePath(s));
        }
    }
    const QString newLogPrefix = newLogName + ".";
    int32_t nextNumber = maxNumber + 1;
    QString candidateLogPath;
    do {
        candidateLogPath = dir.filePath(newLogPrefix + QString::number(nextNumber));
        const QFileDevice::FileError error = setLogFile(candidateLogPath, QIODeviceBase::WriteOnly | QIODeviceBase::NewOnly);
        if (error == QFileDevice::NoError) break;
        if (!QFile::exists(candidateLogPath)) return;
        ++nextNumber;
    } while (true);

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

void Logger::slotWatchLogSize() {
    bool rotateLog = false;
    {
        QMutexLocker lock(&_mutex);
        if (_logstream) {
            _logstream->flush();
            rotateLog = _logFile.size() > CommonUtility::logMaxSize;
        }
    }

    if (rotateLog) {
        kdriveLog("Log too big, archiving current log and creating a new one.");
        enterNextLogFile();
    }
}

bool Logger::compressSingleLog(const QString &sourceName, const QString &targetName) {
    return KDC::CommonUtility::compressFile(sourceName, targetName);
}

} // namespace KDC
