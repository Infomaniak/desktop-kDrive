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

#include <algorithm>
#include <ranges>

namespace {
template<typename IdType>
void appendUnique(std::vector<IdType> &values, const IdType id) {
    const auto insertIt = std::ranges::lower_bound(values, id);
    if (insertIt == values.end() || *insertIt != id) {
        values.insert(insertIt, id);
    }
}

template<typename IdType>
void removeValue(std::vector<IdType> &values, const IdType id) {
    std::erase(values, id);
}
} // namespace

namespace KDC {

void AppCache::linkAccountToUser(const AccountDbId accountDbId, const UserDbId userDbId) {
    if (const auto userIt = _usersByDbId.find(userDbId); userIt != _usersByDbId.end()) {
        appendUnique(userIt->second.accountDbIds, accountDbId);
    }
}

void AppCache::unlinkAccountFromUser(const AccountDbId accountDbId, const UserDbId userDbId) {
    if (const auto userIt = _usersByDbId.find(userDbId); userIt != _usersByDbId.end()) {
        removeValue(userIt->second.accountDbIds, accountDbId);
    }
}

void AppCache::linkDriveToAccount(const DriveDbId driveDbId, const AccountDbId accountDbId) {
    if (const auto accountIt = _accountsByDbId.find(accountDbId); accountIt != _accountsByDbId.end()) {
        appendUnique(accountIt->second.driveDbIds, driveDbId);
    }
}

void AppCache::unlinkDriveFromAccount(const DriveDbId driveDbId, const AccountDbId accountDbId) {
    if (const auto accountIt = _accountsByDbId.find(accountDbId); accountIt != _accountsByDbId.end()) {
        removeValue(accountIt->second.driveDbIds, driveDbId);
    }
}

void AppCache::linkSyncToDrive(const SyncDbId syncDbId, const DriveDbId driveDbId) {
    if (const auto driveIt = _drivesByDbId.find(driveDbId); driveIt != _drivesByDbId.end()) {
        appendUnique(driveIt->second.syncDbIds, syncDbId);
    }
}

void AppCache::unlinkSyncFromDrive(const SyncDbId syncDbId, const DriveDbId driveDbId) {
    if (const auto driveIt = _drivesByDbId.find(driveDbId); driveIt != _drivesByDbId.end()) {
        removeValue(driveIt->second.syncDbIds, syncDbId);
    }
}

void AppCache::linkErrorToSync(const ErrorDbId errorDbId, const SyncDbId syncDbId) {
    if (const auto syncIt = _syncsByDbId.find(syncDbId); syncIt != _syncsByDbId.end()) {
        appendUnique(syncIt->second.errorDbIds, errorDbId);
    }
}

void AppCache::unlinkErrorFromSync(const ErrorDbId errorDbId, const SyncDbId syncDbId) {
    if (const auto syncIt = _syncsByDbId.find(syncDbId); syncIt != _syncsByDbId.end()) {
        removeValue(syncIt->second.errorDbIds, errorDbId);
    }
}

void AppCache::removeAccountCascade(const AccountDbId accountDbId) {
    const auto accountIt = _accountsByDbId.find(accountDbId);
    if (accountIt == _accountsByDbId.end()) {
        return;
    }

    for (const auto driveDbIds = accountIt->second.driveDbIds; const auto driveDbId: driveDbIds) {
        removeDriveCascade(driveDbId);
    }
    unlinkAccountFromUser(accountDbId, accountIt->second.parentUserDbId);
    _accountsByDbId.erase(accountIt);
}

void AppCache::removeDriveCascade(const DriveDbId driveDbId) {
    const auto driveIt = _drivesByDbId.find(driveDbId);
    if (driveIt == _drivesByDbId.end()) {
        return;
    }

    for (const auto syncDbIds = driveIt->second.syncDbIds; const auto syncDbId: syncDbIds) {
        removeSyncCascade(syncDbId);
    }
    unlinkDriveFromAccount(driveDbId, driveIt->second.parentAccountDbId);
    _drivesByDbId.erase(driveIt);
}

void AppCache::removeSyncCascade(const SyncDbId syncDbId) {
    const auto syncIt = _syncsByDbId.find(syncDbId);
    if (syncIt == _syncsByDbId.end()) {
        return;
    }

    for (const auto errorDbId: syncIt->second.errorDbIds) {
        _syncErrorsByDbId.erase(errorDbId);
    }
    unlinkSyncFromDrive(syncDbId, syncIt->second.parentDriveDbId);
    _syncsByDbId.erase(syncIt);
}

void AppCache::pruneConfiguredGraph() {
    std::vector<AccountDbId> orphanAccountIds;
    for (const auto &[accountDbId, node]: _accountsByDbId) {
        if (!_usersByDbId.contains(node.parentUserDbId)) {
            orphanAccountIds.push_back(accountDbId);
        }
    }
    for (const auto accountDbId: orphanAccountIds) {
        removeAccountCascade(accountDbId);
    }

    std::vector<DriveDbId> orphanDriveIds;
    for (const auto &[driveDbId, node]: _drivesByDbId) {
        if (!_accountsByDbId.contains(node.parentAccountDbId)) {
            orphanDriveIds.push_back(driveDbId);
        }
    }
    for (const auto driveDbId: orphanDriveIds) {
        removeDriveCascade(driveDbId);
    }

    std::vector<SyncDbId> orphanSyncIds;
    for (const auto &[syncDbId, node]: _syncsByDbId) {
        if (!_drivesByDbId.contains(node.parentDriveDbId)) {
            orphanSyncIds.push_back(syncDbId);
        }
    }
    for (const auto syncDbId: orphanSyncIds) {
        removeSyncCascade(syncDbId);
    }

    std::erase_if(_availableDrivesByUserDbId, [this](const auto &entry) { return !_usersByDbId.contains(entry.first); });
    rebuildGraphRelations();
}

void AppCache::rebuildGraphRelations() {
    for (auto &userNode: _usersByDbId | std::views::values) {
        userNode.accountDbIds.clear();
    }
    for (auto &accountNode: _accountsByDbId | std::views::values) {
        accountNode.driveDbIds.clear();
    }
    for (auto &driveNode: _drivesByDbId | std::views::values) {
        driveNode.syncDbIds.clear();
    }
    for (auto &syncNode: _syncsByDbId | std::views::values) {
        syncNode.errorDbIds.clear();
    }

    for (const auto &[accountDbId, accountNode]: _accountsByDbId) {
        linkAccountToUser(accountDbId, accountNode.parentUserDbId);
    }
    for (const auto &[driveDbId, driveNode]: _drivesByDbId) {
        linkDriveToAccount(driveDbId, driveNode.parentAccountDbId);
    }
    for (const auto &[syncDbId, syncNode]: _syncsByDbId) {
        linkSyncToDrive(syncDbId, syncNode.parentDriveDbId);
    }
    for (const auto &[errorDbId, errorInfo]: _syncErrorsByDbId) {
        linkErrorToSync(errorDbId, errorInfo.syncDbId());
    }
}

} // namespace KDC
