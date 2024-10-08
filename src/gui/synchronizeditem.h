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

#pragma once

#include "libcommon/utility/types.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>

namespace KDC {

class SynchronizedItem {
    public:
        SynchronizedItem(int syncDbId, const QString &filePath, const QString &fileId, SyncFileStatus status,
                         SyncDirection direction, NodeType type, const QString &fullFilePath, const QDateTime &dateTime,
                         const QString &error = QString());

        inline int syncDbId() const { return _syncDbId; }
        inline QString filePath() const { return _filePath; }
        inline QString fileId() const { return _fileId; }
        inline SyncFileStatus status() const { return _status; }
        inline SyncDirection direction() const { return _direction; }
        inline NodeType type() const { return _type; }
        inline QString fullFilePath() const { return _fullFilePath; }
        inline QDateTime dateTime() const { return _dateTime; }
        inline QString error() const { return _error; }
        inline bool displayed() const { return _displayed; }
        inline void setDisplayed(bool value) { _displayed = value; }

    private:
        int _syncDbId;
        QString _filePath;
        QString _fileId;
        SyncFileStatus _status;
        SyncDirection _direction;
        NodeType _type;
        QString _fullFilePath;
        QDateTime _dateTime;
        QString _error;
        bool _displayed;
};

} // namespace KDC
