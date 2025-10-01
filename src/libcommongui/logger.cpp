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

#include "logger.h"
#include "config.h"
#include "libcommon/utility/utility.h"

#include <QDir>
#include <QStringList>
#include <QThread>
#include <QMap>
#include <QRegularExpression>
#include <QLoggingCategory>

#include <iostream>

#ifdef Q_OS_WIN
#include <io.h> // for stdout
#endif

static const int logSizeWatcherTimeout = 60000;

namespace KDC {

static const QMap<QtMsgType, int> qtMsgTypeLevel = {{QtInfoMsg, 0},     {QtDebugMsg, 1},  {QtWarningMsg, 2},
                                                    {QtCriticalMsg, 3}, {QtSystemMsg, 3}, {QtFatalMsg, 4}};

static void kdriveLogCatcher(QtMsgType type, const QMessageLogContext &ctx, const QString &message) {
    auto logger = Logger::instance();
    if (qtMsgTypeLevel[type] < logger->minLogLevel()) return;

    // Create new context
    SyncName fileName;
    if (ctx.file) {
        const SyncPath filePath(ctx.file);
        fileName = filePath.filename();
    }
#if defined(KD_WINDOWS)
    // For performance purposes, assume that the file name contains only mono byte chars
    std::string unsafeFileName(CommonUtility::toUnsafeStr(fileName));
    const char *fileNamePtr = unsafeFileName.c_str();
#else
    const char *fileNamePtr = fileName.c_str();
#endif
    QMessageLogContext ctxNew(fileNamePtr, ctx.line, ctx.function, ctx.category);

    if (!logger->isNoop()) {
        logger->doLog(qFormatLogMessage(type, ctxNew, message));
    } else if (type >= QtCriticalMsg) {
        std::cerr << qPrintable(qFormatLogMessage(type, ctxNew, message)) << std::endl;
    }

#if defined(Q_OS_WIN)
    // Make application terminate in a way that can be caught by the crash reporter
    if (type == QtFatalMsg) {
        KDC::CommonUtility::crash();
    }
#endif
}


Logger *Logger::instance() {
    static Logger log;
    return &log;
}

Logger::Logger(QObject *parent) :
    QObject(parent),
    _showTime(true),
    _doFileFlush(false),
    _logExpire(0),
    _logDebug(false) {
    qSetMessagePattern(
            "%{time yyyy-MM-dd hh:mm:ss:zzz} "
            "[%{if-debug}D%{endif}%{if-info}I%{endif}%{if-warning}W%{endif}%{if-critical}C%{endif}%{if-fatal}F%{endif}] "
            "(%{threadid}) %{file}:%{line} - %{message}");
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
}

void Logger::setIsCLientLog(bool newIsCLientLog) {
    _isCLientLog = newIsCLientLog;
}

int Logger::minLogLevel() const {
    return _minLogLevel;
}

void Logger::setMinLogLevel(int level) {
    _minLogLevel = level;
}

void Logger::postNotification(const QString &title, const QString &message) {
    emit showNotification(title, message);
}

void Logger::log(Log log) {
    QString msg;
    if (_showTime) {
        msg = log.timeStamp.toString(QLatin1String("MM-dd hh:mm:ss:zzz")) + QLatin1Char(' ');
    }

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

void Logger::doLog(const QString &msg) {
    {
        QMutexLocker lock(&_mutex);
        if (_logstream) {
            (*_logstream) << msg << Qt::endl;
            if (_doFileFlush) _logstream->flush();
        }
    }
    emit logWindowLog(msg);
}

void Logger::kdriveLog(const QString &message) {
    Log log_;
    log_.timeStamp = QDateTime::currentDateTimeUtc();
    log_.message = message;

    Logger::instance()->log(log_);
}

void Logger::setLogFile(const QString &name) {
    QMutexLocker locker(&_mutex);
    if (_logstream) {
        _logstream.reset(0);
        _logFile.close();
    }

    if (name.isEmpty()) {
        return;
    }

    bool openSucceeded = false;
    if (name == QLatin1String("-")) {
        openSucceeded = _logFile.open(stdout, QIODevice::WriteOnly);
    } else {
        _logFile.setFileName(name);
        openSucceeded = _logFile.open(QIODevice::WriteOnly);
    }

    if (!openSucceeded) {
        locker.unlock(); // Just in case postGuiMessage has a qDebug()
        postNotification(tr("Error"), QString(tr("<nobr>File '%1'<br/>cannot be opened for writing.<br/><br/>"
                                                 "The log output can <b>not</b> be saved!</nobr>"))
                                              .arg(name));
        return;
    }

    _logstream.reset(new QTextStream(&_logFile));
}

void Logger::setLogExpire(std::chrono::hours expire) {
    _logExpire = expire;
}

void Logger::setLogDir(const QString &dir) {
    _logDirectory = dir;
}

void Logger::setLogFlush(bool flush) {
    _doFileFlush = flush;
}

void Logger::setLogDebug(bool debug) {
    QLoggingCategory::setFilterRules(debug ? QStringLiteral("*=false\nsync.*=true\nsync.database.sql=false\nserver.*=true\ngui.*="
                                                            "true\ncommon.*=true\nlibcommon.*=true\nvfs.*=true")
                                           : QString());
    _logDebug = debug;
}

QString Logger::temporaryFolderLogDirPath() const {
    static const QString dirName = APPLICATION_NAME + QString("-logdir");

    QDir kDriveTempDirectory;
    if (const auto &value = CommonUtility::envVarValue("KDRIVE_TMP_PATH"); !value.empty()) {
        kDriveTempDirectory = QDir(QString::fromStdString(value));
    } else {
        kDriveTempDirectory = QDir::temp();
    }

    return kDriveTempDirectory.filePath(dirName);
}

void Logger::setupTemporaryFolderLogDir() {
    auto dir = temporaryFolderLogDirPath();
    if (!QDir().mkpath(dir)) return;
    setLogDebug(true);
    setLogDir(dir);
    _temporaryFolderLogDir = true;
}

void Logger::disableTemporaryFolderLogDir() {
    if (!_temporaryFolderLogDir) return;

    enterNextLogFile();
    setLogDir(QString());
    setLogDebug(false);
    setLogFile(QString());
    _temporaryFolderLogDir = false;
}

void Logger::enterNextLogFile() {
    if (!_logDirectory.isEmpty()) {
        QDir dir(_logDirectory);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // Tentative new log name, will be adjusted if one like this already exists
        QDateTime now = QDateTime::currentDateTime();
        QString appName(APPLICATION_NAME);
        if (_isCLientLog) {
            appName += QString("_client");
        }
        QString newLogName = now.toString("yyyyMMdd_HHmm") + QString("_%1.log").arg(appName);

        // Expire old log files and deal with conflicts
        QStringList unzippedFiles;
        QStringList files = dir.entryList(QStringList(QString("*%1.log.*").arg(appName)), QDir::Files, QDir::Name);
        QString rxPattern(QString(R"(.*%1\.log\.(\d+).*)").arg(appName));
        rxPattern = QRegularExpression::anchoredPattern(rxPattern);

        QString unzippedPattern(QString(R"(.*%1\.log\.\d$)").arg(appName));
        unzippedPattern = QRegularExpression::anchoredPattern(unzippedPattern);

        int maxNumber = -1;
        foreach (const QString &s, files) {
            if (_logExpire.count() > 0) {
                std::chrono::seconds expireSeconds(_logExpire);
                QFileInfo fileInfo(dir.absoluteFilePath(s));
                if (fileInfo.lastModified().addSecs(expireSeconds.count()) < now) {
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
}

void Logger::slotWatchLogSize() {
    if (_isCLientLog) {
        // Do not check log size from client
        _watchLogSizeTimer.stop();
    } else {
        if (_logFile.size() > CommonUtility::logMaxSize) {
            kdriveLog("Log too big, archiving current log and creating a new one.");
            emit logTooBig();
            enterNextLogFile();
        }
    }
}

bool Logger::compressSingleLog(const QString &sourceName, const QString &targetName) {
    return KDC::CommonUtility::compressFile(sourceName, targetName);
}

} // namespace KDC
