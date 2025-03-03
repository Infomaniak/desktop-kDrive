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

#include "config.h"

#include <QString>

#include <ctime>

class QFile;

namespace KDC {

/**
 *  \addtogroup libsync
 *  @{
 */

/**
 * @brief This file contains file system helper
 */
namespace FileSystem {

/**
 * @brief Mark the file as hidden  (only has effects on windows)
 */
void setFileHidden(const QString &filename, bool hidden);

/**
 * @brief Marks the file as read-only.
 *
 * On linux this either revokes all 'w' permissions or restores permissions
 * according to the umask.
 */
void setFileReadOnly(const QString &filename, bool readonly);

/**
 * @brief Marks the file as read-only.
 *
 * It's like setFileReadOnly(), but weaker: if readonly is false and the user
 * already has write permissions, no change to the permissions is made.
 *
 * This means that it will preserve explicitly set rw-r--r-- permissions even
 * when the umask is 0002. (setFileReadOnly() would adjust to rw-rw-r--)
 */
void setFileReadOnlyWeak(const QString &filename, bool readonly);

/**
 * @brief Set user write permission to file.
 */
void setUserWritePermission(const QString &filename);

/**
 * @brief Try to set permissions so that other users on the local machine can not
 * go into the folder.
 */
void setFolderMinimumPermissions(const QString &filename);

/** convert a "normal" windows path into a path that can be 32k chars long. */
QString longWinPath(const QString &inpath);

/**
 * @brief Rename the file \a originFileName to \a destinationFileName.
 *
 * It behaves as QFile::rename() but handles .lnk files correctly on Windows.
 */
bool rename(const QString &originFileName, const QString &destinationFileName, QString *errorString = nullptr);

/**
 * Removes a file.
 *
 * Equivalent to QFile::remove(), except on Windows, where it will also
 * successfully remove read-only files.
 */
bool remove(const QString &fileName, QString *errorString = 0);

/**
 * Move the specified file or folder to the trash. (Only implemented on linux)
 */
bool moveToTrash(const QString &filename, QString *errorString);

/**
 * Replacement for QFile::open(ReadOnly) followed by a seek().
 * This version sets a more permissive sharing mode on Windows.
 *
 * Warning: The resulting file may have an empty fileName and be unsuitable for use
 * with QFileInfo! Calling seek() on the QFile with >32bit signed values will fail!
 */
bool openAndSeekFileSharedRead(QFile *file, QString *error, qint64 seek);

/**
 * Returns true when a file is locked. (Windows only)
 */
bool isFileLocked(const QString &fileName);

/**
 * Returns whether the file is a shortcut file (ends with .lnk)
 */
bool isLnkFile(const QString &filename);

/*
 * This function takes a path and converts it to a UNC representation of the
 * string. That means that it prepends a \\?\ (unless already UNC) and converts
 * all slashes to backslashes.
 *
 * Note the following:
 *  - The string must be absolute.
 *  - it needs to contain a drive character to be a valid UNC
 *  - A conversion is only done if the path len is larger than 245. Otherwise,
 *    the Windows API functions work with the normal "unixoid" representation too.
 */
template<typename S>
S pathtoUNC(const S &str) {
    int len = 0;
    S longStr;

    len = str.length();
    longStr.reserve(len + 4);

    // prepend \\?\ and convert '/' => '\' to support long names
    if (str[0] == '/' || str[0] == '\\') {
        // Don't prepend if already UNC
        if (!(len > 1 && (str[1] == '/' || str[1] == '\\'))) {
            longStr.append("\\\\?");
        }
    } else {
        longStr.append("\\\\?\\"); // prepend string by this four magic chars.
    }
    longStr += str;

    /* replace all occurences of / with the windows native \ */

    for (auto it = longStr.begin(); it != longStr.end(); ++it) {
        if (*it == '/') {
            *it = '\\';
        }
    }
    return longStr;
}
} // namespace FileSystem

/** @} */
} // namespace KDC
