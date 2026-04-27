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

#pragma once

#include "app/cache/appcache.h"
#include "app/cache/cachetypes.h"

#include <QLoggingCategory>
#include <QObject>

Q_DECLARE_LOGGING_CATEGORY(lcOnboardingState)

#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace KDC {

/**
 * Onboarding-only state store for Linux v4.
 *
 * Role: own selected onboarding user, selected available drive keys, and pending sync configs.
 * Non-role: own main-shell current sync or configured cache entities.
 * All mutations must run on the Qt main thread.
 */
class OnboardingState : public QObject {
        Q_OBJECT
        Q_PROPERTY(qint64 selectedUserDbId READ selectedUserDbId NOTIFY selectedUserDbIdChanged)

    public:
        explicit OnboardingState(AppCache &cache, QObject *parent = nullptr);

        [[nodiscard]] qint64 selectedUserDbId() const;
        [[nodiscard]] UserDbId typedSelectedUserDbId() const { return _selectedUserDbId; }
        [[nodiscard]] std::vector<AvailableDriveKey> selectedAvailableDriveKeys() const;
        [[nodiscard]] bool isAvailableDriveSelected(const AvailableDriveKey &key) const;
        [[nodiscard]] std::optional<PendingSyncConfig> pendingSyncConfig(const AvailableDriveKey &key) const;

        Q_INVOKABLE void selectUser(qint64 userDbId);
        Q_INVOKABLE void clearSelectedUser();
        void selectAvailableDrive(const AvailableDriveKey &key);
        void unselectAvailableDrive(const AvailableDriveKey &key);
        void toggleAvailableDrive(const AvailableDriveKey &key);
        void clearSelectedAvailableDrives();
        void setPendingSyncConfig(const AvailableDriveKey &key, const PendingSyncConfig &config);
        void clearPendingSyncConfig(const AvailableDriveKey &key);
        void reset();

    signals:
        void selectedUserDbIdChanged();
        void selectedAvailableDrivesChanged();
        void pendingSyncConfigsChanged();

    private:
        [[nodiscard]] bool belongsToSelectedUser(const AvailableDriveKey &key) const;
        void ensureValidState();
        void pruneSelectedAvailableDrives();

        AppCache &_cache;
        UserDbId _selectedUserDbId{0};
        std::unordered_set<AvailableDriveKey> _selectedAvailableDriveKeys;
        std::unordered_map<AvailableDriveKey, PendingSyncConfig> _pendingSyncConfigs;
};

} // namespace KDC
