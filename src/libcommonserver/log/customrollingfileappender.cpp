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

#include "config.h"
#include "customrollingfileappender.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/utility/utility.h"

/*****************************************/
/********** namespace log4cplus **********/
/*****************************************/

// Module:  Log4CPLUS
// File:    fileappender.cxx
// Created: 6/2001
// Author:  Tad E. Smith
//
//
// Copyright 2001-2017 Tad E. Smith
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <log4cplus/fileappender.h>
#include <log4cplus/helpers/loglog.h>
#include <log4cplus/helpers/property.h>
#include <log4cplus/helpers/fileinfo.h>
#include <log4cplus/spi/factory.h>

#include <sstream>

// For _wrename() and _wremove() on Windows.
#include <stdio.h>
#include <cerrno>
#ifdef LOG4CPLUS_HAVE_ERRNO_H
#include <errno.h>
#endif

namespace log4cplus {

using helpers::Properties;
using helpers::Time;

///////////////////////////////////////////////////////////////////////////////
// File LOCAL definitions
///////////////////////////////////////////////////////////////////////////////

namespace {

long const LOG4CPLUS_FILE_NOT_FOUND = ENOENT;

static long file_rename(tstring const &src, tstring const &target) {
#if defined(UNICODE) && defined(KD_WINDOWS)
    if (_wrename(src.c_str(), target.c_str()) == 0)
        return 0;
    else
        return errno;
#else
    if (std::rename(LOG4CPLUS_TSTRING_TO_STRING(src).c_str(), LOG4CPLUS_TSTRING_TO_STRING(target).c_str()) == 0)
        return 0;
    else
        return errno;
#endif
}

static long file_remove(tstring const &src) {
#if defined(UNICODE) && defined(KD_WINDOWS)
    if (_wremove(src.c_str()) == 0)
        return 0;
    else
        return errno;
#else
    if (std::remove(LOG4CPLUS_TSTRING_TO_STRING(src).c_str()) == 0)
        return 0;
    else
        return errno;
#endif
}

static void loglog_renaming_result(helpers::LogLog &loglog, tstring const &src, tstring const &target, long ret) {
    if (ret == 0) {
        loglog.debug(LOG4CPLUS_TEXT("Renamed file ") + src + LOG4CPLUS_TEXT(" to ") + target);
    } else if (ret != LOG4CPLUS_FILE_NOT_FOUND) {
        tostringstream oss;
        oss << LOG4CPLUS_TEXT("Failed to rename file from ") << src << LOG4CPLUS_TEXT(" to ") << target
            << LOG4CPLUS_TEXT("; error ") << ret;
        loglog.error(oss.str());
    }
}

static void loglog_opening_result(helpers::LogLog &loglog, log4cplus::tostream const &os, tstring const &filename) {
    if (!os) {
        loglog.error(LOG4CPLUS_TEXT("Failed to open file ") + filename);
    }
}

} // namespace

} // namespace log4cplus

/***********************************/
/********** namespace KDC **********/
/***********************************/

namespace KDC {

const log4cplus::tstring empty_str;

static void rolloverFiles(const log4cplus::tstring &filename, int maxBackupIndex) {
    log4cplus::helpers::LogLog *loglog = log4cplus::helpers::LogLog::getLogLog();

    // Delete the oldest file
    log4cplus::tostringstream buffer;
    buffer << filename << LOG4CPLUS_TEXT(".") << maxBackupIndex << LOG4CPLUS_TEXT(".gz");
    long ret = log4cplus::file_remove(buffer.str());

    log4cplus::tostringstream source_oss;
    log4cplus::tostringstream target_oss;

    // Map {(maxBackupIndex - 1), ..., 2, 1} to {maxBackupIndex, ..., 3, 2}
    for (int i = maxBackupIndex - 1; i >= 1; --i) {
        source_oss.str(empty_str);
        target_oss.str(empty_str);

        source_oss << filename << LOG4CPLUS_TEXT(".") << i << LOG4CPLUS_TEXT(".gz");
        target_oss << filename << LOG4CPLUS_TEXT(".") << (i + 1) << LOG4CPLUS_TEXT(".gz");

        log4cplus::tstring const source(source_oss.str());
        log4cplus::tstring const target(target_oss.str());

#if defined(KD_WINDOWS)
        // Try to remove the target first. It seems it is not
        // possible to rename over existing file.
        ret = log4cplus::file_remove(target);
#endif

        ret = log4cplus::file_rename(source, target);
        log4cplus::loglog_renaming_result(*loglog, source, target, ret);
    }
}

CustomRollingFileAppender::CustomRollingFileAppender(const log4cplus::tstring &filename, long maxFileSize, int maxBackupIndex,
                                                     bool immediateFlush, bool createDirs) :
    RollingFileAppender(filename, LONG_MAX /*Let us handle a custom rollover*/, maxBackupIndex, immediateFlush, createDirs),
    _maxFileSize(maxFileSize),
    _lastExpireCheck() {}

CustomRollingFileAppender::CustomRollingFileAppender(const log4cplus::helpers::Properties &properties) :
    RollingFileAppender(properties),
    _lastExpireCheck() {}

void CustomRollingFileAppender::append(const log4cplus::spi::InternalLoggingEvent &event) {
    // Seek to the end of log file so that tellp() below returns the
    // right size.
    if (useLockFile) out.seekp(0, std::ios_base::end);

    // Rotate log file if needed before appending to it.
    if (out.tellp() > _maxFileSize) customRollover(true);

    try {
        RollingFileAppender::append(event);
    } catch (...) {
        // Bug in gcc => std::filesystem::path wstring is crashing with non ASCII characters
        // Fixed in next gcc-12 version
        // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=95048
        return;
    }

    // Rotate log file if needed after appending to it.
    if (out.tellp() > _maxFileSize) customRollover(true);
}

void CustomRollingFileAppender::customRollover(bool alreadyLocked) {
    log4cplus::helpers::LogLog &loglog = log4cplus::helpers::getLogLog();
    log4cplus::helpers::LockFileGuard guard;

    // Close the current file
    out.close();
    // Reset flags since the C++ standard specified that all the flags
    // should remain unchanged on a close.
    out.clear();

    if (useLockFile) {
        if (!alreadyLocked) {
            try {
                guard.attach_and_lock(*lockFile);
            } catch (std::runtime_error const &) {
                return;
            }
        }

        // Recheck the condition as there is a window where another
        // process can rollover the file before us.

        log4cplus::helpers::FileInfo fi;
        if (getFileInfo(&fi, filename) == -1 || fi.size < maxFileSize) {
            // The file has already been rolled by another
            // process. Just reopen with the new file.

            // Open it up again.
            open(std::ios_base::out | std::ios_base::ate | std::ios_base::app);
            log4cplus::loglog_opening_result(loglog, out, filename);

            return;
        }
    }

    // If maxBackups <= 0, then there is no file renaming to be done.
    if (maxBackupIndex > 0) {
        rolloverFiles(filename, maxBackupIndex);

        // Rename fileName to fileName.1
        log4cplus::tstring target = filename + LOG4CPLUS_TEXT(".1");

        long ret = 0;

#if defined(KD_WINDOWS)
        // Try to remove the target first. It seems it is not
        // possible to rename over existing file.
        ret = log4cplus::file_remove(target);
#endif

        loglog.debug(LOG4CPLUS_TEXT("Renaming file ") + filename + LOG4CPLUS_TEXT(" to ") + target);
        ret = log4cplus::file_rename(filename, target);
        log4cplus::loglog_renaming_result(loglog, filename, target, ret);

        // Compress file
        log4cplus::tstring ztarget = target + LOG4CPLUS_TEXT(".gz");

        bool exists;
        IoError ioError = IoError::Success;
        const bool success = IoHelper::checkIfPathExists(ztarget, exists, ioError);
        if (!success) {
            loglog.debug(filename + LOG4CPLUS_TEXT(" failed to check if path exists"));
        }

        if (exists) {
            log4cplus::file_remove(ztarget);
        }

        if (success && CommonUtility::compressFile(target, ztarget)) {
            log4cplus::file_remove(target);
        } else {
            log4cplus::file_remove(ztarget);
        }
    } else {
        loglog.debug(filename + LOG4CPLUS_TEXT(" has no backups specified"));
    }

    // Open it up again in truncation mode
    open(std::ios::out | std::ios::trunc);
    log4cplus::loglog_opening_result(loglog, out, filename);
}

void CustomRollingFileAppender::checkForExpiredFiles() {
    _lastExpireCheck = std::chrono::system_clock::now();
    // Archive previous log files and delete expired files
    IoError ioError = IoError::Success;
    SyncPath logDirPath;
    if (!IoHelper::logDirectoryPath(logDirPath, ioError) || ioError != IoError::Success) {
        return;
    }
    IoHelper::DirectoryIterator dirIt(logDirPath, false, ioError);
    bool endOfDir = false;
    DirectoryEntry entry;
    while (dirIt.next(entry, endOfDir, ioError) && !endOfDir && ioError == IoError::Success) {
        FileStat fileStat;
        IoHelper::getFileStat(entry.path(), &fileStat, ioError);
        if (ioError != IoError::Success || fileStat.nodeType != NodeType::File) {
            continue;
        }

        // Delete expired files
        if (_expire > 0 && entry.path().string().find(APPLICATION_NAME) != std::string::npos) {
            const auto now = std::chrono::system_clock::now();
            const auto lastModified = std::chrono::system_clock::from_time_t(fileStat.modificationTime); // Only 1s precision.
            const auto expireDateTime = lastModified + std::chrono::seconds(_expire);
            if (expireDateTime < now) {
                log4cplus::file_remove(CommonUtility::s2ws(entry.path().string()));
                continue;
            }
        }

        // Compress previous log sessions
        if (const SyncPath currentLogName = DirectoryEntry(filename).path().filename().replace_extension("");
            entry.path().filename().string().find(".gz") == std::string::npos &&
            entry.path().string().find(currentLogName.string()) == std::string::npos) {
            if (CommonUtility::compressFile(entry.path().string(), entry.path().string() + ".gz")) {
                log4cplus::file_remove(CommonUtility::s2ws(entry.path().string()));
            } else {
                log4cplus::file_remove(CommonUtility::s2ws(entry.path().string()) + LOG4CPLUS_TEXT(".gz"));
            }
        }
    }
}
} // namespace KDC
