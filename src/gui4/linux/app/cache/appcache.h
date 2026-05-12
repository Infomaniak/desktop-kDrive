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

#include "app/cache/cachetypes.h"

#include <QLoggingCategory>
#include <QObject>

#include <optional>
#include <unordered_map>
#include <vector>

Q_DECLARE_LOGGING_CATEGORY(lcAppCache)

namespace KDC {

/**
 * Durable graph-backed cache and query layer for Linux v4 product state.
 *
 * Role: own configured entities, per-user available drives, graph relations, and derived read models.
 * All mutations must run on the Qt main thread.
 */
class AppCache : public QObject {
        Q_OBJECT

    public:
        explicit AppCache(QObject *parent = nullptr);

        // Flat snapshots rebuilt from the canonical graph for compatibility with list consumers.
        [[nodiscard]] std::vector<UserInfo> users() const;
        [[nodiscard]] std::vector<AccountInfo> accounts() const;
        [[nodiscard]] std::vector<DriveInfo> drives() const;
        [[nodiscard]] std::vector<SyncInfo> syncs() const;
        [[nodiscard]] std::vector<ErrorInfo> syncErrors() const;
        [[nodiscard]] std::vector<ErrorInfo> serverErrors() const;
        // Returns the addable-drive snapshot scoped to one user. This is not tied to main selection.
        [[nodiscard]] std::vector<DriveAvailableInfo> availableDrives(UserDbId userDbId) const;

        // Direct id-based lookups. Missing or orphaned entities are returned as std::nullopt.
        [[nodiscard]] std::optional<UserInfo> user(UserDbId userDbId) const;
        [[nodiscard]] std::optional<AccountInfo> account(AccountDbId accountDbId) const;
        [[nodiscard]] std::optional<DriveInfo> drive(DriveDbId driveDbId) const;
        [[nodiscard]] std::optional<SyncInfo> sync(SyncDbId syncDbId) const;
        [[nodiscard]] std::optional<ErrorInfo> syncError(ErrorDbId errorDbId) const;
        [[nodiscard]] std::optional<ErrorInfo> serverError(ErrorDbId errorDbId) const;
        [[nodiscard]] std::optional<DriveAvailableInfo> availableDrive(const AvailableDriveKey &key) const;

        // Parent-to-children graph traversal helpers. Results are stable-sorted by database id.
        [[nodiscard]] std::vector<AccountInfo> accountsForUser(UserDbId userDbId) const;
        [[nodiscard]] std::vector<DriveInfo> drivesForAccount(AccountDbId accountDbId) const;
        [[nodiscard]] std::vector<SyncInfo> syncsForDrive(DriveDbId driveDbId) const;
        // Sync-scoped errors sorted by error time, oldest first.
        [[nodiscard]] std::vector<ErrorInfo> errorsForSync(SyncDbId syncDbId) const;

        // Derived read models used by sidebar, settings, and onboarding adapters.
        // A context is omitted when its full parent chain cannot be resolved.
        [[nodiscard]] std::optional<SyncContext> syncContext(SyncDbId syncDbId) const;
        [[nodiscard]] std::vector<SyncContext> syncContexts() const;
        [[nodiscard]] std::optional<DriveContext> driveContext(DriveDbId driveDbId) const;
        [[nodiscard]] std::vector<DriveContext> driveContexts() const;
        // Available-drive contexts reconcile addable drives with configured drives for display/disable decisions.
        [[nodiscard]] std::vector<AvailableDriveContext> availableDriveContexts(UserDbId userDbId) const;
        [[nodiscard]] std::vector<AvailableDriveContext> availableDriveContexts() const;

        // Clears all cached product state. Transport/connection state is intentionally not represented here.
        void clearAll();

        // Atomic family snapshot replacements. Orphans are pruned so the graph remains coherent.
        void replaceUsers(const std::vector<UserDisplayInfo> &users);
        void replaceAccounts(const std::vector<AccountInfo> &accounts);
        void replaceDrives(const std::vector<DriveInfo> &drives);
        void replaceSyncs(const std::vector<SyncInfo> &syncs);
        void replaceSyncErrors(const std::vector<ErrorInfo> &errors);
        void replaceServerErrors(const std::vector<ErrorInfo> &errors);
        // Replaces only one user's addable-drive snapshot; other users' snapshots are preserved.
        void replaceAvailableDrivesForUser(UserDbId userDbId, const std::vector<DriveAvailableInfo> &availableDrives);
        void clearAvailableDrivesForUser(UserDbId userDbId);
        void clearAllAvailableDrives();

    public slots:
        // Incremental entity mutations used by push-signal wiring. Remove methods own descendant cascade cleanup.
        void upsertUser(const UserDisplayInfo &info);
        void removeUser(UserDbId userDbId);

        void upsertAccount(const AccountInfo &info);
        void removeAccount(AccountDbId accountDbId);

        void upsertDrive(const DriveInfo &info);
        void removeDrive(DriveDbId driveDbId);

        void upsertSync(const SyncInfo &info);
        void removeSync(SyncDbId syncDbId);

        void upsertSyncError(const ErrorInfo &info);
        void removeSyncError(ErrorDbId errorDbId);
        void upsertServerError(const ErrorInfo &info);
        void removeServerError(ErrorDbId errorDbId);
        void upsertError(const ErrorInfo &info);
        void removeError(ErrorDbId errorDbId);

    signals:
        // Coarse invalidation signals. Current QML-facing adapters should rebuild their read models from queries.
        void usersChanged();
        void accountsChanged();
        void drivesChanged();
        void syncsChanged();
        void syncErrorsChanged();
        void serverErrorsChanged();
        void availableDrivesChanged(UserDbId userDbId);
        void allAvailableDrivesChanged();

    private:
        struct UserNode {
                UserDisplayInfo info;
                std::vector<AccountDbId> accountDbIds;
        };

        struct AccountNode {
                AccountInfo info;
                UserDbId parentUserDbId{0};
                std::vector<DriveDbId> driveDbIds;
        };

        struct DriveNode {
                DriveInfo info;
                AccountDbId parentAccountDbId{0};
                std::vector<SyncDbId> syncDbIds;
        };

        struct SyncNode {
                SyncInfo info;
                DriveDbId parentDriveDbId{0};
                std::vector<ErrorDbId> errorDbIds;
        };

        // Maintains canonical parent-child relation lists when entities move or are replaced.
        void linkAccountToUser(AccountDbId accountDbId, UserDbId userDbId);
        void unlinkAccountFromUser(AccountDbId accountDbId, UserDbId userDbId);
        void linkDriveToAccount(DriveDbId driveDbId, AccountDbId accountDbId);
        void unlinkDriveFromAccount(DriveDbId driveDbId, AccountDbId accountDbId);
        void linkSyncToDrive(SyncDbId syncDbId, DriveDbId driveDbId);
        void unlinkSyncFromDrive(SyncDbId syncDbId, DriveDbId driveDbId);
        void linkErrorToSync(ErrorDbId errorDbId, SyncDbId syncDbId);
        void unlinkErrorFromSync(ErrorDbId errorDbId, SyncDbId syncDbId);

        // Cascade helpers remove descendants without emitting signals; public removals emit the final invalidations.
        void removeAccountCascade(AccountDbId accountDbId);
        void removeDriveCascade(DriveDbId driveDbId);
        void removeSyncCascade(SyncDbId syncDbId);

        // Prunes orphan entities after snapshot replacement and rebuilds child id lists from canonical maps.
        void pruneConfiguredGraph();
        void rebuildGraphRelations();

        // Resolve configured-account/drive matches for addable-drive read models.
        [[nodiscard]] std::optional<AccountInfo> accountForAvailableDrive(UserDbId userDbId, AccountId accountId) const;
        [[nodiscard]] std::optional<DriveInfo> configuredDriveForAvailableDrive(AccountDbId accountDbId, DriveId driveId) const;

        std::unordered_map<UserDbId, UserNode> _usersByDbId;
        std::unordered_map<AccountDbId, AccountNode> _accountsByDbId;
        std::unordered_map<DriveDbId, DriveNode> _drivesByDbId;
        std::unordered_map<SyncDbId, SyncNode> _syncsByDbId;
        std::unordered_map<ErrorDbId, ErrorInfo> _syncErrorsByDbId;
        std::unordered_map<ErrorDbId, ErrorInfo> _serverErrorsByDbId;
        std::unordered_map<UserDbId, std::vector<DriveAvailableInfo>> _availableDrivesByUserDbId;
};

} // namespace KDC
