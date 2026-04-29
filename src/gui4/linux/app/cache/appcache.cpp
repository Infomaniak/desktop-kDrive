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


#include "appcache.h"

#include <ranges>
#include <utility>

namespace KDC {

AppCache::AppCache(QObject *const parent) :
    QObject(parent) {}

void AppCache::clearAll() {
    qCInfo(lcAppCache) << "Cache cleared";
    const bool hadUsers = !_usersByDbId.empty();
    const bool hadAccounts = !_accountsByDbId.empty();
    const bool hadDrives = !_drivesByDbId.empty();
    const bool hadSyncs = !_syncsByDbId.empty();
    const bool hadSyncErrors = !_syncErrorsByDbId.empty();
    const bool hadServerErrors = !_serverErrorsByDbId.empty();
    const bool hadAvailableDrives = !_availableDrivesByUserDbId.empty();

    _usersByDbId.clear();
    _accountsByDbId.clear();
    _drivesByDbId.clear();
    _syncsByDbId.clear();
    _syncErrorsByDbId.clear();
    _serverErrorsByDbId.clear();
    _availableDrivesByUserDbId.clear();

    if (hadUsers) emit usersChanged();
    if (hadAccounts) emit accountsChanged();
    if (hadDrives) emit drivesChanged();
    if (hadSyncs) emit syncsChanged();
    if (hadSyncErrors) emit syncErrorsChanged();
    if (hadServerErrors) emit serverErrorsChanged();
    if (hadAvailableDrives) emit allAvailableDrivesChanged();
}

void AppCache::replaceUsers(const std::vector<UserInfo> &users) {
    _usersByDbId.clear();
    for (const auto &info: users) {
        _usersByDbId[info.dbId()].info = info;
    }
    pruneConfiguredGraph();
    emit usersChanged();
    emit accountsChanged();
    emit drivesChanged();
    emit syncsChanged();
    emit syncErrorsChanged();
    emit allAvailableDrivesChanged();
}

void AppCache::replaceAccounts(const std::vector<AccountInfo> &accounts) {
    for (auto &userNode: _usersByDbId | std::views::values) {
        userNode.accountDbIds.clear();
    }
    _accountsByDbId.clear();
    for (const auto &info: accounts) {
        if (!_usersByDbId.contains(info.userDbId())) {
            qCWarning(lcAppCache) << "Account dropped during replace | accountDbId:" << info.dbId()
                                  << "/ unknown userDbId:" << info.userDbId();
            continue;
        }
        _accountsByDbId[info.dbId()] = AccountNode{info, info.userDbId(), {}};
    }
    pruneConfiguredGraph();
    emit accountsChanged();
    emit drivesChanged();
    emit syncsChanged();
    emit syncErrorsChanged();
}

void AppCache::replaceDrives(const std::vector<DriveInfo> &drives) {
    for (auto &accountNode: _accountsByDbId | std::views::values) {
        accountNode.driveDbIds.clear();
    }
    _drivesByDbId.clear();
    for (const auto &info: drives) {
        if (!_accountsByDbId.contains(info.accountDbId())) {
            qCWarning(lcAppCache) << "Drive dropped during replace | driveDbId:" << info.dbId()
                                  << "/ unknown accountDbId:" << info.accountDbId();
            continue;
        }
        _drivesByDbId[info.dbId()] = DriveNode{info, info.accountDbId(), {}};
    }
    pruneConfiguredGraph();
    emit drivesChanged();
    emit syncsChanged();
    emit syncErrorsChanged();
}

void AppCache::replaceSyncs(const std::vector<SyncInfo> &syncs) {
    for (auto &driveNode: _drivesByDbId | std::views::values) {
        driveNode.syncDbIds.clear();
    }
    _syncsByDbId.clear();
    for (const auto &info: syncs) {
        if (!_drivesByDbId.contains(info.driveDbId())) {
            qCWarning(lcAppCache) << "Sync dropped during replace | syncDbId:" << info.dbId()
                                  << "/ unknown driveDbId:" << info.driveDbId();
            continue;
        }
        _syncsByDbId[info.dbId()] = SyncNode{info, info.driveDbId(), {}};
    }
    pruneConfiguredGraph();
    emit syncsChanged();
    emit syncErrorsChanged();
}

void AppCache::replaceSyncErrors(const std::vector<ErrorInfo> &errors) {
    for (auto &syncNode: _syncsByDbId | std::views::values) {
        syncNode.errorDbIds.clear();
    }
    _syncErrorsByDbId.clear();
    for (const auto &info: errors) {
        if (!_syncsByDbId.contains(info.syncDbId())) {
            qCWarning(lcAppCache) << "Sync error dropped during replace | errorDbId:" << info.dbId()
                                  << "/ unknown syncDbId:" << info.syncDbId();
            continue;
        }
        _syncErrorsByDbId[info.dbId()] = info;
        linkErrorToSync(info.dbId(), info.syncDbId());
    }
    emit syncErrorsChanged();
}

void AppCache::replaceServerErrors(const std::vector<ErrorInfo> &errors) {
    _serverErrorsByDbId.clear();
    for (const auto &info: errors) {
        _serverErrorsByDbId[info.dbId()] = info;
    }
    emit serverErrorsChanged();
}

void AppCache::replaceAvailableDrivesForUser(const UserDbId userDbId, const std::vector<DriveAvailableInfo> &availableDrives) {
    std::vector<DriveAvailableInfo> scopedAvailableDrives;
    scopedAvailableDrives.reserve(availableDrives.size());
    for (auto info: availableDrives) {
        info.setUserDbId(userDbId);
        scopedAvailableDrives.push_back(info);
    }
    _availableDrivesByUserDbId[userDbId] = std::move(scopedAvailableDrives);
    emit availableDrivesChanged(userDbId);
}

void AppCache::clearAvailableDrivesForUser(const UserDbId userDbId) {
    if (_availableDrivesByUserDbId.erase(userDbId) == 0) {
        return;
    }
    emit availableDrivesChanged(userDbId);
}

void AppCache::clearAllAvailableDrives() {
    if (_availableDrivesByUserDbId.empty()) {
        return;
    }
    _availableDrivesByUserDbId.clear();
    emit allAvailableDrivesChanged();
}

void AppCache::upsertUser(const UserInfo &info) {
    auto &node = _usersByDbId[info.dbId()];
    node.info = info;
    qCDebug(lcAppCache) << "User upserted | dbId:" << info.dbId();
    emit usersChanged();
}

void AppCache::removeUser(const UserDbId userDbId) {
    const auto userIt = _usersByDbId.find(userDbId);
    if (userIt == _usersByDbId.end()) {
        return;
    }
    qCDebug(lcAppCache) << "User removed | dbId:" << userDbId;

    for (const auto accountDbIds = userIt->second.accountDbIds; const auto accountDbId: accountDbIds) {
        removeAccountCascade(accountDbId);
    }
    (void) _usersByDbId.erase(userIt);
    const bool hadAvailableDrives = _availableDrivesByUserDbId.erase(userDbId) > 0;

    emit usersChanged();
    emit accountsChanged();
    emit drivesChanged();
    emit syncsChanged();
    emit syncErrorsChanged();
    if (hadAvailableDrives) emit availableDrivesChanged(userDbId);
}

void AppCache::upsertAccount(const AccountInfo &info) {
    if (!_usersByDbId.contains(info.userDbId())) {
        qCWarning(lcAppCache) << "Account upsert dropped | accountDbId:" << info.dbId()
                              << "/ unknown userDbId:" << info.userDbId();
        return;
    }

    if (const auto existingIt = _accountsByDbId.find(info.dbId());
        existingIt != _accountsByDbId.end() && existingIt->second.parentUserDbId != info.userDbId()) {
        unlinkAccountFromUser(info.dbId(), existingIt->second.parentUserDbId);
    }

    auto &node = _accountsByDbId[info.dbId()];
    node.info = info;
    node.parentUserDbId = info.userDbId();
    linkAccountToUser(info.dbId(), info.userDbId());
    qCDebug(lcAppCache) << "Account upserted | dbId:" << info.dbId() << "/ userDbId:" << info.userDbId();
    emit accountsChanged();
}

void AppCache::removeAccount(const AccountDbId accountDbId) {
    if (!_accountsByDbId.contains(accountDbId)) {
        return;
    }
    qCDebug(lcAppCache) << "Account removed | dbId:" << accountDbId;
    removeAccountCascade(accountDbId);
    emit accountsChanged();
    emit drivesChanged();
    emit syncsChanged();
    emit syncErrorsChanged();
}

void AppCache::upsertDrive(const DriveInfo &info) {
    if (!_accountsByDbId.contains(info.accountDbId())) {
        qCWarning(lcAppCache) << "Drive upsert dropped | driveDbId:" << info.dbId()
                              << "/ unknown accountDbId:" << info.accountDbId();
        return;
    }

    if (const auto existingIt = _drivesByDbId.find(info.dbId());
        existingIt != _drivesByDbId.end() && existingIt->second.parentAccountDbId != info.accountDbId()) {
        unlinkDriveFromAccount(info.dbId(), existingIt->second.parentAccountDbId);
    }

    auto &node = _drivesByDbId[info.dbId()];
    node.info = info;
    node.parentAccountDbId = info.accountDbId();
    linkDriveToAccount(info.dbId(), info.accountDbId());
    qCDebug(lcAppCache) << "Drive upserted | dbId:" << info.dbId() << "/ accountDbId:" << info.accountDbId();
    emit drivesChanged();
}

void AppCache::removeDrive(const DriveDbId driveDbId) {
    if (!_drivesByDbId.contains(driveDbId)) {
        return;
    }
    qCDebug(lcAppCache) << "Drive removed | dbId:" << driveDbId;
    removeDriveCascade(driveDbId);
    emit drivesChanged();
    emit syncsChanged();
    emit syncErrorsChanged();
}

void AppCache::upsertSync(const SyncInfo &info) {
    if (!_drivesByDbId.contains(info.driveDbId())) {
        qCWarning(lcAppCache) << "Sync upsert dropped | syncDbId:" << info.dbId() << "/ unknown driveDbId:" << info.driveDbId();
        return;
    }

    if (const auto existingIt = _syncsByDbId.find(info.dbId());
        existingIt != _syncsByDbId.end() && existingIt->second.parentDriveDbId != info.driveDbId()) {
        unlinkSyncFromDrive(info.dbId(), existingIt->second.parentDriveDbId);
    }

    auto &node = _syncsByDbId[info.dbId()];
    node.info = info;
    node.parentDriveDbId = info.driveDbId();
    linkSyncToDrive(info.dbId(), info.driveDbId());
    qCDebug(lcAppCache) << "Sync upserted | dbId:" << info.dbId() << "/ driveDbId:" << info.driveDbId();
    emit syncsChanged();
}

void AppCache::removeSync(const SyncDbId syncDbId) {
    if (!_syncsByDbId.contains(syncDbId)) {
        return;
    }
    qCDebug(lcAppCache) << "Sync removed | dbId:" << syncDbId;
    removeSyncCascade(syncDbId);
    emit syncsChanged();
    emit syncErrorsChanged();
}

void AppCache::upsertSyncError(const ErrorInfo &info) {
    if (!_syncsByDbId.contains(info.syncDbId())) {
        qCWarning(lcAppCache) << "Sync error upsert dropped | errorDbId:" << info.dbId()
                              << "/ unknown syncDbId:" << info.syncDbId();
        return;
    }

    if (const auto existingIt = _syncErrorsByDbId.find(info.dbId());
        existingIt != _syncErrorsByDbId.end() && existingIt->second.syncDbId() != info.syncDbId()) {
        unlinkErrorFromSync(info.dbId(), existingIt->second.syncDbId());
    }

    _syncErrorsByDbId[info.dbId()] = info;
    linkErrorToSync(info.dbId(), info.syncDbId());
    qCDebug(lcAppCache) << "Sync error upserted | dbId:" << info.dbId() << "/ syncDbId:" << info.syncDbId();
    emit syncErrorsChanged();
}

void AppCache::removeSyncError(const ErrorDbId errorDbId) {
    const auto errorIt = _syncErrorsByDbId.find(errorDbId);
    if (errorIt == _syncErrorsByDbId.end()) {
        return;
    }
    qCDebug(lcAppCache) << "Sync error removed | dbId:" << errorDbId;
    unlinkErrorFromSync(errorDbId, errorIt->second.syncDbId());
    (void) _syncErrorsByDbId.erase(errorIt);
    emit syncErrorsChanged();
}

void AppCache::upsertServerError(const ErrorInfo &info) {
    _serverErrorsByDbId[info.dbId()] = info;
    emit serverErrorsChanged();
}

void AppCache::removeServerError(const ErrorDbId errorDbId) {
    if (_serverErrorsByDbId.erase(errorDbId) == 0) {
        return;
    }
    emit serverErrorsChanged();
}

} // namespace KDC
