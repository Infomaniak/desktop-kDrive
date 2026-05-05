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

#include "currentsyncmodel.h"

namespace KDC {

CurrentSyncModel::CurrentSyncModel(MainSelectionStore &mainSelectionStore, QObject *const parent) :
    QObject(parent),
    _mainSelectionStore(mainSelectionStore),
    _context(_mainSelectionStore.currentSyncContext()) {
    (void) connect(&_mainSelectionStore, &MainSelectionStore::currentSyncContextChanged, this, &CurrentSyncModel::refreshContext);
}

bool CurrentSyncModel::hasSync() const {
    return _context.has_value();
}

qint64 CurrentSyncModel::syncDbId() const {
    return _context ? static_cast<qint64>(_context->sync.dbId()) : 0;
}

qint64 CurrentSyncModel::driveDbId() const {
    return _context ? static_cast<qint64>(_context->drive.dbId()) : 0;
}

qint64 CurrentSyncModel::driveId() const {
    return _context ? static_cast<qint64>(_context->drive.id()) : 0;
}

QString CurrentSyncModel::driveName() const {
    return _context ? _context->drive.name() : QString{};
}

qint64 CurrentSyncModel::accountDbId() const {
    return _context ? static_cast<qint64>(_context->account.dbId()) : 0;
}

qint64 CurrentSyncModel::accountId() const {
    return _context ? static_cast<qint64>(_context->account.id()) : 0;
}

QString CurrentSyncModel::accountName() const {
    return _context ? QString::fromStdString(_context->account.name()) : QString{};
}

qint64 CurrentSyncModel::userDbId() const {
    return _context ? static_cast<qint64>(_context->user.dbId()) : 0;
}

qint64 CurrentSyncModel::userId() const {
    return _context ? static_cast<qint64>(_context->user.userId()) : 0;
}

QString CurrentSyncModel::userName() const {
    return _context ? _context->user.name() : QString{};
}

QString CurrentSyncModel::userEmail() const {
    return _context ? _context->user.email() : QString{};
}

QString CurrentSyncModel::localPath() const {
    return _context ? _context->sync.localPath() : QString{};
}

QString CurrentSyncModel::targetPath() const {
    return _context ? _context->sync.targetPath() : QString{};
}

QString CurrentSyncModel::targetNodeId() const {
    return _context ? _context->sync.targetNodeId() : QString{};
}

bool CurrentSyncModel::supportVfs() const {
    return _context ? _context->sync.supportVfs() : false;
}

qint32 CurrentSyncModel::virtualFileMode() const {
    return _context ? static_cast<qint32>(_context->sync.virtualFileMode()) : 0;
}

QString CurrentSyncModel::navigationPaneClsid() const {
    return _context ? _context->sync.navigationPaneClsid() : QString{};
}

qint64 CurrentSyncModel::errorCount() const {
    return _context ? static_cast<qint64>(_context->errors.size()) : 0;
}

bool CurrentSyncModel::hasError() const {
    return _context && _context->latestError.has_value();
}

qint64 CurrentSyncModel::latestErrorDbId() const {
    return hasError() ? static_cast<qint64>(_context->latestError->dbId()) : 0;
}

qint64 CurrentSyncModel::latestErrorTime() const {
    return hasError() ? _context->latestError->getTime() : 0;
}

qint32 CurrentSyncModel::latestErrorLevel() const {
    return hasError() ? static_cast<qint32>(_context->latestError->level()) : 0;
}

qint32 CurrentSyncModel::latestErrorExitCode() const {
    return hasError() ? static_cast<qint32>(_context->latestError->exitCode()) : 0;
}

QString CurrentSyncModel::latestErrorPath() const {
    return hasError() ? _context->latestError->path() : QString{};
}

void CurrentSyncModel::refreshContext() {
    auto nextContext = _mainSelectionStore.currentSyncContext();
    if (_context == nextContext) {
        return;
    }

    _context = std::move(nextContext);
    emit contextChanged();
}

} // namespace KDC
