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

#include "onboardingstate.h"

#include <algorithm>

namespace KDC {

OnboardingState::OnboardingState(AppCache &cache, QObject *const parent) :
    QObject(parent),
    _cache(cache) {
    (void) connect(&_cache, &AppCache::usersChanged, this, &OnboardingState::ensureValidState);
    (void) connect(&_cache, &AppCache::availableDrivesChanged, this, [this](const UserDbId userDbId) {
        if (userDbId == _selectedUserDbId) {
            pruneSelectedAvailableDrives();
        }
    });
    (void) connect(&_cache, &AppCache::allAvailableDrivesChanged, this, &OnboardingState::pruneSelectedAvailableDrives);
}

qint64 OnboardingState::selectedUserDbId() const {
    return static_cast<qint64>(_selectedUserDbId);
}

std::vector<AvailableDriveKey> OnboardingState::selectedAvailableDriveKeys() const {
    std::vector<AvailableDriveKey> keys(_selectedAvailableDriveKeys.begin(), _selectedAvailableDriveKeys.end());
    std::ranges::sort(keys, [](const AvailableDriveKey &lhs, const AvailableDriveKey &rhs) {
        if (lhs.userDbId != rhs.userDbId) return lhs.userDbId < rhs.userDbId;
        if (lhs.accountId != rhs.accountId) return lhs.accountId < rhs.accountId;
        return lhs.driveId < rhs.driveId;
    });
    return keys;
}

bool OnboardingState::isAvailableDriveSelected(const AvailableDriveKey &key) const {
    return _selectedAvailableDriveKeys.contains(key);
}

std::optional<PendingSyncConfig> OnboardingState::pendingSyncConfig(const AvailableDriveKey &key) const {
    const auto it = _pendingSyncConfigs.find(key);
    if (it == _pendingSyncConfigs.end()) {
        return std::nullopt;
    }
    return it->second;
}

void OnboardingState::selectUser(const qint64 userDbId) {
    const auto typedUserDbId = static_cast<UserDbId>(userDbId);
    if (_selectedUserDbId == typedUserDbId) {
        return;
    }

    const bool hadSelectedDrives = !_selectedAvailableDriveKeys.empty();
    const bool hadPendingConfigs = !_pendingSyncConfigs.empty();
    _selectedUserDbId = typedUserDbId;
    _selectedAvailableDriveKeys.clear();
    _pendingSyncConfigs.clear();

    emit selectedUserDbIdChanged();
    if (hadSelectedDrives) emit selectedAvailableDrivesChanged();
    if (hadPendingConfigs) emit pendingSyncConfigsChanged();
}

void OnboardingState::clearSelectedUser() {
    selectUser(0);
}

void OnboardingState::selectAvailableDrive(const AvailableDriveKey &key) {
    if (!belongsToSelectedUser(key)) {
        return;
    }
    if (!_selectedAvailableDriveKeys.insert(key).second) {
        return;
    }
    emit selectedAvailableDrivesChanged();
}

void OnboardingState::unselectAvailableDrive(const AvailableDriveKey &key) {
    if (_selectedAvailableDriveKeys.erase(key) == 0) {
        return;
    }
    const bool removedConfig = _pendingSyncConfigs.erase(key) > 0;
    emit selectedAvailableDrivesChanged();
    if (removedConfig) emit pendingSyncConfigsChanged();
}

void OnboardingState::toggleAvailableDrive(const AvailableDriveKey &key) {
    if (isAvailableDriveSelected(key)) {
        unselectAvailableDrive(key);
        return;
    }
    selectAvailableDrive(key);
}

void OnboardingState::clearSelectedAvailableDrives() {
    const bool hadSelectedDrives = !_selectedAvailableDriveKeys.empty();
    const bool hadPendingConfigs = !_pendingSyncConfigs.empty();
    _selectedAvailableDriveKeys.clear();
    _pendingSyncConfigs.clear();
    if (hadSelectedDrives) emit selectedAvailableDrivesChanged();
    if (hadPendingConfigs) emit pendingSyncConfigsChanged();
}

void OnboardingState::setPendingSyncConfig(const AvailableDriveKey &key, const PendingSyncConfig &config) {
    if (!belongsToSelectedUser(key) || !isAvailableDriveSelected(key)) {
        return;
    }
    _pendingSyncConfigs[key] = config;
    emit pendingSyncConfigsChanged();
}

void OnboardingState::clearPendingSyncConfig(const AvailableDriveKey &key) {
    if (_pendingSyncConfigs.erase(key) == 0) {
        return;
    }
    emit pendingSyncConfigsChanged();
}

void OnboardingState::reset() {
    const bool userChanged = _selectedUserDbId != 0;
    const bool hadSelectedDrives = !_selectedAvailableDriveKeys.empty();
    const bool hadPendingConfigs = !_pendingSyncConfigs.empty();
    _selectedUserDbId = 0;
    _selectedAvailableDriveKeys.clear();
    _pendingSyncConfigs.clear();
    if (userChanged) emit selectedUserDbIdChanged();
    if (hadSelectedDrives) emit selectedAvailableDrivesChanged();
    if (hadPendingConfigs) emit pendingSyncConfigsChanged();
}

bool OnboardingState::belongsToSelectedUser(const AvailableDriveKey &key) const {
    return _selectedUserDbId != 0 && key.userDbId == _selectedUserDbId;
}

void OnboardingState::ensureValidState() {
    if (_selectedUserDbId == 0 || _cache.user(_selectedUserDbId)) {
        pruneSelectedAvailableDrives();
        return;
    }
    reset();
}

void OnboardingState::pruneSelectedAvailableDrives() {
    if (_selectedAvailableDriveKeys.empty()) {
        return;
    }

    bool selectionChanged = false;
    bool pendingConfigsChanged = false;
    for (auto it = _selectedAvailableDriveKeys.begin(); it != _selectedAvailableDriveKeys.end();) {
        if (_cache.availableDrive(*it)) {
            ++it;
            continue;
        }
        pendingConfigsChanged = _pendingSyncConfigs.erase(*it) > 0 || pendingConfigsChanged;
        it = _selectedAvailableDriveKeys.erase(it);
        selectionChanged = true;
    }

    if (selectionChanged) emit selectedAvailableDrivesChanged();
    if (pendingConfigsChanged) emit pendingSyncConfigsChanged();
}

} // namespace KDC
