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

#include "synchronizeditem.h"

namespace KDC {

SynchronizedItem::SynchronizedItem(int syncDbId, const QString &filePath, const QString &fileId, SyncFileStatus status,
                                   SyncDirection direction, NodeType type, const QString &fullFilePath, const QDateTime &dateTime,
                                   const QString &error) :
    _syncDbId(syncDbId), _filePath(filePath), _fileId(fileId), _status(status), _direction(direction), _type(type),
    _fullFilePath(fullFilePath), _dateTime(dateTime), _error(error), _displayed(false) {}

} // namespace KDC
