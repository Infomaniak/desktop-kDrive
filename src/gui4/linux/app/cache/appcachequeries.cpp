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
template<typename T, typename Getter>
void sortById(std::vector<T> &values, Getter getter) {
    (void) std::ranges::sort(values, [getter](const T &lhs, const T &rhs) { return getter(lhs) < getter(rhs); });
}
} // namespace

namespace KDC {

std::vector<UserInfo> AppCache::users() const {
    std::vector<UserInfo> values;
    values.reserve(_usersByDbId.size());
    for (const auto &node: _usersByDbId | std::views::values) {
        values.push_back(node.info);
    }
    sortById(values, [](const UserInfo &info) { return info.dbId(); });
    return values;
}

std::vector<AccountInfo> AppCache::accounts() const {
    std::vector<AccountInfo> values;
    values.reserve(_accountsByDbId.size());
    for (const auto &node: _accountsByDbId | std::views::values) {
        values.push_back(node.info);
    }
    sortById(values, [](const AccountInfo &info) { return info.dbId(); });
    return values;
}

std::vector<DriveInfo> AppCache::drives() const {
    std::vector<DriveInfo> values;
    values.reserve(_drivesByDbId.size());
    for (const auto &node: _drivesByDbId | std::views::values) {
        values.push_back(node.info);
    }
    sortById(values, [](const DriveInfo &info) { return info.dbId(); });
    return values;
}

std::vector<SyncInfo> AppCache::syncs() const {
    std::vector<SyncInfo> values;
    values.reserve(_syncsByDbId.size());
    for (const auto &node: _syncsByDbId | std::views::values) {
        values.push_back(node.info);
    }
    sortById(values, [](const SyncInfo &info) { return info.dbId(); });
    return values;
}

std::vector<ErrorInfo> AppCache::syncErrors() const {
    std::vector<ErrorInfo> values;
    values.reserve(_syncErrorsByDbId.size());
    for (const auto &info: _syncErrorsByDbId | std::views::values) {
        values.push_back(info);
    }
    sortById(values, [](const ErrorInfo &info) { return info.dbId(); });
    return values;
}

std::vector<ErrorInfo> AppCache::serverErrors() const {
    std::vector<ErrorInfo> values;
    values.reserve(_serverErrorsByDbId.size());
    for (const auto &info: _serverErrorsByDbId | std::views::values) {
        values.push_back(info);
    }
    sortById(values, [](const ErrorInfo &info) { return info.dbId(); });
    return values;
}

std::vector<DriveAvailableInfo> AppCache::availableDrives(const UserDbId userDbId) const {
    const auto it = _availableDrivesByUserDbId.find(userDbId);
    if (it == _availableDrivesByUserDbId.end()) {
        return {};
    }
    auto values = it->second;
    (void) std::ranges::sort(values, [](const DriveAvailableInfo &lhs, const DriveAvailableInfo &rhs) {
        if (lhs.accountId() != rhs.accountId()) {
            return lhs.accountId() < rhs.accountId();
        }
        return lhs.driveId() < rhs.driveId();
    });
    return values;
}

std::optional<UserInfo> AppCache::user(const UserDbId userDbId) const {
    const auto it = _usersByDbId.find(userDbId);
    if (it == _usersByDbId.end()) {
        return std::nullopt;
    }
    return it->second.info;
}

std::optional<AccountInfo> AppCache::account(const AccountDbId accountDbId) const {
    const auto it = _accountsByDbId.find(accountDbId);
    if (it == _accountsByDbId.end()) {
        return std::nullopt;
    }
    return it->second.info;
}

std::optional<DriveInfo> AppCache::drive(const DriveDbId driveDbId) const {
    const auto it = _drivesByDbId.find(driveDbId);
    if (it == _drivesByDbId.end()) {
        return std::nullopt;
    }
    return it->second.info;
}

std::optional<SyncInfo> AppCache::sync(const SyncDbId syncDbId) const {
    const auto it = _syncsByDbId.find(syncDbId);
    if (it == _syncsByDbId.end()) {
        return std::nullopt;
    }
    return it->second.info;
}

std::optional<ErrorInfo> AppCache::syncError(const ErrorDbId errorDbId) const {
    const auto it = _syncErrorsByDbId.find(errorDbId);
    if (it == _syncErrorsByDbId.end()) {
        return std::nullopt;
    }
    if (!_syncsByDbId.contains(it->second.syncDbId())) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<ErrorInfo> AppCache::serverError(const ErrorDbId errorDbId) const {
    const auto it = _serverErrorsByDbId.find(errorDbId);
    if (it == _serverErrorsByDbId.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<DriveAvailableInfo> AppCache::availableDrive(const AvailableDriveKey &key) const {
    const auto it = _availableDrivesByUserDbId.find(key.userDbId);
    if (it == _availableDrivesByUserDbId.end()) {
        return std::nullopt;
    }

    const auto availableDriveIt = std::ranges::find_if(it->second, [&key](const DriveAvailableInfo &info) {
        return info.accountId() == key.accountId && info.driveId() == key.driveId;
    });
    if (availableDriveIt == it->second.end()) {
        return std::nullopt;
    }
    return *availableDriveIt;
}

std::vector<AccountInfo> AppCache::accountsForUser(const UserDbId userDbId) const {
    const auto userIt = _usersByDbId.find(userDbId);
    if (userIt == _usersByDbId.end()) {
        return {};
    }

    std::vector<AccountInfo> values;
    values.reserve(userIt->second.accountDbIds.size());
    for (const auto accountDbId: userIt->second.accountDbIds) {
        if (const auto accountInfo = account(accountDbId)) {
            values.push_back(*accountInfo);
        }
    }
    sortById(values, [](const AccountInfo &info) { return info.dbId(); });
    return values;
}

std::vector<DriveInfo> AppCache::drivesForAccount(const AccountDbId accountDbId) const {
    const auto accountIt = _accountsByDbId.find(accountDbId);
    if (accountIt == _accountsByDbId.end()) {
        return {};
    }

    std::vector<DriveInfo> values;
    values.reserve(accountIt->second.driveDbIds.size());
    for (const auto driveDbId: accountIt->second.driveDbIds) {
        if (const auto driveInfo = drive(driveDbId)) {
            values.push_back(*driveInfo);
        }
    }
    sortById(values, [](const DriveInfo &info) { return info.dbId(); });
    return values;
}

std::vector<SyncInfo> AppCache::syncsForDrive(const DriveDbId driveDbId) const {
    const auto driveIt = _drivesByDbId.find(driveDbId);
    if (driveIt == _drivesByDbId.end()) {
        return {};
    }

    std::vector<SyncInfo> values;
    values.reserve(driveIt->second.syncDbIds.size());
    for (const auto syncDbId: driveIt->second.syncDbIds) {
        if (const auto syncInfo = sync(syncDbId)) {
            values.push_back(*syncInfo);
        }
    }
    sortById(values, [](const SyncInfo &info) { return info.dbId(); });
    return values;
}

std::vector<ErrorInfo> AppCache::errorsForSync(const SyncDbId syncDbId) const {
    const auto syncIt = _syncsByDbId.find(syncDbId);
    if (syncIt == _syncsByDbId.end()) {
        return {};
    }

    std::vector<ErrorInfo> values;
    values.reserve(syncIt->second.errorDbIds.size());
    for (const auto errorDbId: syncIt->second.errorDbIds) {
        if (const auto errorInfo = syncError(errorDbId)) {
            values.push_back(*errorInfo);
        }
    }
    (void) std::ranges::sort(values, [](const ErrorInfo &lhs, const ErrorInfo &rhs) { return lhs.getTime() < rhs.getTime(); });
    return values;
}

std::optional<SyncContext> AppCache::syncContext(const SyncDbId syncDbId) const {
    const auto syncIt = _syncsByDbId.find(syncDbId);
    if (syncIt == _syncsByDbId.end()) {
        return std::nullopt;
    }

    const auto driveIt = _drivesByDbId.find(syncIt->second.parentDriveDbId);
    if (driveIt == _drivesByDbId.end()) {
        return std::nullopt;
    }

    const auto accountIt = _accountsByDbId.find(driveIt->second.parentAccountDbId);
    if (accountIt == _accountsByDbId.end()) {
        return std::nullopt;
    }

    const auto userIt = _usersByDbId.find(accountIt->second.parentUserDbId);
    if (userIt == _usersByDbId.end()) {
        return std::nullopt;
    }

    SyncContext context;
    context.userDisplayInfo = userIt->second.info;
    context.accountInfo = accountIt->second.info;
    context.driveInfo = driveIt->second.info;
    context.syncInfo = syncIt->second.info;
    context.errorInfoList = errorsForSync(syncDbId);
    if (!context.errorInfoList.empty()) {
        context.latestErrorInfo = context.errorInfoList.back();
    }
    return context;
}

std::vector<SyncContext> AppCache::syncContexts() const {
    std::vector<SyncContext> contexts;
    contexts.reserve(_syncsByDbId.size());
    for (const auto syncDbId: _syncsByDbId | std::views::keys) {
        if (const auto context = syncContext(syncDbId)) {
            contexts.push_back(*context);
        }
    }
    (void) std::ranges::sort(contexts, [](const SyncContext &lhs, const SyncContext &rhs) {
        if (lhs.userDisplayInfo.dbId() != rhs.userDisplayInfo.dbId())
            return lhs.userDisplayInfo.dbId() < rhs.userDisplayInfo.dbId();
        if (lhs.accountInfo.dbId() != rhs.accountInfo.dbId()) return lhs.accountInfo.dbId() < rhs.accountInfo.dbId();
        if (lhs.driveInfo.dbId() != rhs.driveInfo.dbId()) return lhs.driveInfo.dbId() < rhs.driveInfo.dbId();
        return lhs.syncInfo.dbId() < rhs.syncInfo.dbId();
    });
    return contexts;
}

std::optional<DriveContext> AppCache::driveContext(const DriveDbId driveDbId) const {
    const auto driveIt = _drivesByDbId.find(driveDbId);
    if (driveIt == _drivesByDbId.end()) {
        return std::nullopt;
    }

    const auto accountIt = _accountsByDbId.find(driveIt->second.parentAccountDbId);
    if (accountIt == _accountsByDbId.end()) {
        return std::nullopt;
    }

    const auto userIt = _usersByDbId.find(accountIt->second.parentUserDbId);
    if (userIt == _usersByDbId.end()) {
        return std::nullopt;
    }

    DriveContext context;
    context.userDisplayInfo = userIt->second.info;
    context.accountInfo = accountIt->second.info;
    context.driveInfo = driveIt->second.info;
    context.syncInfos = syncsForDrive(driveDbId);
    return context;
}

std::vector<DriveContext> AppCache::driveContexts() const {
    std::vector<DriveContext> contexts;
    contexts.reserve(_drivesByDbId.size());
    for (const auto driveDbId: _drivesByDbId | std::views::keys) {
        if (const auto context = driveContext(driveDbId)) {
            contexts.push_back(*context);
        }
    }
    (void) std::ranges::sort(contexts, [](const DriveContext &lhs, const DriveContext &rhs) {
        if (lhs.userDisplayInfo.dbId() != rhs.userDisplayInfo.dbId())
            return lhs.userDisplayInfo.dbId() < rhs.userDisplayInfo.dbId();
        if (lhs.accountInfo.dbId() != rhs.accountInfo.dbId()) return lhs.accountInfo.dbId() < rhs.accountInfo.dbId();
        return lhs.driveInfo.dbId() < rhs.driveInfo.dbId();
    });
    return contexts;
}

std::vector<AvailableDriveContext> AppCache::availableDriveContexts(const UserDbId userDbId) const {
    const auto userInfo = user(userDbId);
    if (!userInfo) {
        return {};
    }

    std::vector<AvailableDriveContext> contexts;
    for (const auto &availableDrive: availableDrives(userDbId)) {
        AvailableDriveContext context;
        context.userDisplayInfo = _usersByDbId.at(userDbId).info;
        context.availableDriveInfo = availableDrive;
        context.accountInfo = accountForAvailableDrive(userDbId, availableDrive.accountId());
        if (context.accountInfo) {
            context.configuredDriveInfo = configuredDriveForAvailableDrive(context.accountInfo->dbId(), availableDrive.driveId());
            context.alreadyConfigured = context.configuredDriveInfo.has_value();
        }
        contexts.push_back(context);
    }
    return contexts;
}

std::vector<AvailableDriveContext> AppCache::availableDriveContexts() const {
    std::vector<AvailableDriveContext> contexts;
    for (const auto userDbId: _availableDrivesByUserDbId | std::views::keys) {
        auto userContexts = availableDriveContexts(userDbId);
        (void) contexts.insert(contexts.end(), userContexts.begin(), userContexts.end());
    }
    (void) std::ranges::sort(contexts, [](const AvailableDriveContext &lhs, const AvailableDriveContext &rhs) {
        if (lhs.userDisplayInfo.dbId() != rhs.userDisplayInfo.dbId())
            return lhs.userDisplayInfo.dbId() < rhs.userDisplayInfo.dbId();
        if (lhs.availableDriveInfo.accountId() != rhs.availableDriveInfo.accountId()) {
            return lhs.availableDriveInfo.accountId() < rhs.availableDriveInfo.accountId();
        }
        return lhs.availableDriveInfo.driveId() < rhs.availableDriveInfo.driveId();
    });
    return contexts;
}

std::optional<AccountInfo> AppCache::accountForAvailableDrive(const UserDbId userDbId, const AccountId accountId) const {
    const auto userIt = _usersByDbId.find(userDbId);
    if (userIt == _usersByDbId.end()) {
        return std::nullopt;
    }

    for (const auto accountDbId: userIt->second.accountDbIds) {
        if (const auto accountIt = _accountsByDbId.find(accountDbId);
            accountIt != _accountsByDbId.end() && accountIt->second.info.id() == accountId) {
            return accountIt->second.info;
        }
    }
    return std::nullopt;
}

std::optional<DriveInfo> AppCache::configuredDriveForAvailableDrive(const AccountDbId accountDbId, const DriveId driveId) const {
    const auto accountIt = _accountsByDbId.find(accountDbId);
    if (accountIt == _accountsByDbId.end()) {
        return std::nullopt;
    }

    for (const auto driveDbId: accountIt->second.driveDbIds) {
        if (const auto driveIt = _drivesByDbId.find(driveDbId);
            driveIt != _drivesByDbId.end() && driveIt->second.info.id() == driveId) {
            return driveIt->second.info;
        }
    }
    return std::nullopt;
}

} // namespace KDC
