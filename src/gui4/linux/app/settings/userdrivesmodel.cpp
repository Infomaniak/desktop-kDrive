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

#include "userdrivesmodel.h"

#include "app/appconstants.h"

#include <QLoggingCategory>

#include <algorithm>
#include <map>
#include <ranges>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcUserDrivesModel, "gui.v4.userdrivesmodel", QtInfoMsg)

[[nodiscard]] QColor driveColor(const std::string &value) {
    const QColor color{QString::fromStdString(value)};
    return color.isValid() ? color : AppConstants::Drive::defaultColor();
}

[[nodiscard]] QString availableDriveAccountName(const AvailableDriveContext &context) {
    if (!context.availableDrive.accountName().empty()) {
        return QString::fromStdString(context.availableDrive.accountName());
    }

    return context.accountInfo.has_value() ? QString::fromStdString(context.accountInfo->name()) : QString();
}

} // namespace

UserDrivesModel::UserDrivesModel(const AppCache &cache, const UserDbId userDbId, QObject *const parent) :
    QAbstractListModel(parent),
    _cache(cache),
    _userDbId(userDbId) {
    (void) connect(&_cache, &AppCache::accountsChanged, this, &UserDrivesModel::rebuild);
    (void) connect(&_cache, &AppCache::drivesChanged, this, &UserDrivesModel::rebuild);
    (void) connect(&_cache, &AppCache::syncsChanged, this, &UserDrivesModel::rebuild);
    (void) connect(&_cache, &AppCache::availableDrivesChanged, this, [this](const UserDbId changedUserDbId) {
        if (changedUserDbId == _userDbId) {
            rebuild();
        }
    });
    (void) connect(&_cache, &AppCache::allAvailableDrivesChanged, this, &UserDrivesModel::rebuild);
    (void) connect(&_cache, &AppCache::syncCreationPendingChanged, this, &UserDrivesModel::rebuild);

    rebuild();
}

int UserDrivesModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(_entries.size());
}

QVariant UserDrivesModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto &entry = _entries[static_cast<std::size_t>(index.row())];
    switch (role) {
        case NameRole:
        case Qt::DisplayRole:
            return entry.name;
        case AccountNameRole:
            return entry.accountName;
        case ColorRole:
            return entry.color;
        case SynchronizedRole:
            return entry.synchronized;
        case AccountIdRole:
            return QVariant::fromValue<qint64>(entry.accountId);
        case DriveIdRole:
            return QVariant::fromValue<qint64>(entry.driveId);
        case SyncCreationPendingRole:
            return entry.syncCreationPending;
        case DriveDbIdRole:
            return QVariant::fromValue<qint64>(entry.driveDbId);
        default:
            return {};
    }
}

QHash<int, QByteArray> UserDrivesModel::roleNames() const {
    return {
            {NameRole, "name"},
            {AccountNameRole, "accountName"},
            {ColorRole, "color"},
            {SynchronizedRole, "isSynchronized"},
            {AccountIdRole, "accountId"},
            {DriveIdRole, "driveId"},
            {SyncCreationPendingRole, "syncCreationPending"},
            {DriveDbIdRole, "driveDbId"},
    };
}

void UserDrivesModel::rebuild() {
    const auto entryLessThan = [](const DriveEntry &lhs, const DriveEntry &rhs) {
        return QString::compare(lhs.name, rhs.name, Qt::CaseInsensitive) < 0;
    };

    using DriveKey = std::pair<AccountId, DriveId>;
    std::map<DriveKey, DriveEntry> synchronizedByKey;
    std::map<DriveKey, DriveEntry> availableByKey;
    // Retains the cache identity of the configured row selected for each backend drive identity, for duplicate diagnostics.
    std::map<DriveKey, DriveDbId> configuredDriveDbIds;

    for (const auto &context: _cache.driveContexts()) {
        if (context.userDisplayInfo.dbId() != _userDbId) {
            continue;
        }

        // A drive owning only advanced synchronizations is synchronized too: its page offers to enable the main one.
        if (context.syncInfos.empty()) {
            continue;
        }

        const auto classicSyncCount = std::ranges::count_if(
                context.syncInfos, [](const BaseSync &syncInfo) { return syncInfo.targetNodeId().empty(); });
        if (classicSyncCount > 1) {
            const auto mainSync = _cache.mainSync(context.drive.dbId());
            qCWarning(lcUserDrivesModel) << "Multiple classic synchronizations found for drive | driveDbId:"
                                         << context.drive.dbId() << "/ mainSyncDbId:" << (mainSync ? mainSync->dbId() : 0);
        }

        const DriveKey key{context.accountInfo.accountId(), context.drive.driveId()};
        const DriveEntry entry{
                .name = QString::fromStdString(context.drive.name()),
                .accountName = QString::fromStdString(context.accountInfo.name()),
                .color = driveColor(context.drive.color()),
                .synchronized = true,
                .accountId = context.accountInfo.accountId(),
                .driveId = context.drive.driveId(),
                .driveDbId = context.drive.dbId(),
        };
        if (const auto [pair, inserted] = synchronizedByKey.try_emplace(key, entry); !inserted) {
            qCWarning(lcUserDrivesModel) << "Duplicate configured drive found for user card | userDbId:" << _userDbId
                                         << "/ accountId:" << key.first << "/ driveId:" << key.second
                                         << "/ keptDriveDbId:" << configuredDriveDbIds.at(key);
        } else if (!configuredDriveDbIds.try_emplace(key, context.drive.dbId()).second) {
            qCWarning(lcUserDrivesModel) << "Configured drive identity index is inconsistent | userDbId:" << _userDbId
                                         << "/ accountId:" << key.first << "/ driveId:" << key.second;
        }
    }

    for (const auto &context: _cache.availableDriveContexts(_userDbId)) {
        if (context.alreadyConfigured) {
            continue;
        }

        const DriveKey key{context.availableDrive.accountId(), context.availableDrive.driveId()};
        if (synchronizedByKey.contains(key)) {
            continue;
        }

        (void) availableByKey.try_emplace(key, DriveEntry{
                                                       .name = QString::fromStdString(context.availableDrive.name()),
                                                       .accountName = availableDriveAccountName(context),
                                                       .color = driveColor(context.availableDrive.color()),
                                                       .syncCreationPending = context.syncCreationPending,
                                                       .accountId = context.availableDrive.accountId(),
                                                       .driveId = context.availableDrive.driveId(),
                                               });
    }

    std::vector<DriveEntry> synchronizedEntries;
    synchronizedEntries.reserve(synchronizedByKey.size());
    for (auto &entry: synchronizedByKey | std::views::values) {
        synchronizedEntries.push_back(std::move(entry));
    }

    std::vector<DriveEntry> availableEntries;
    availableEntries.reserve(availableByKey.size());
    for (auto &entry: availableByKey | std::views::values) {
        availableEntries.push_back(std::move(entry));
    }

    // Maps provide deterministic AccountId/DriveId ordering for equal case-insensitive names.
    (void) std::ranges::stable_sort(synchronizedEntries, entryLessThan);
    (void) std::ranges::stable_sort(availableEntries, entryLessThan);
    (void) synchronizedEntries.insert(synchronizedEntries.end(), availableEntries.begin(), availableEntries.end());

    if (synchronizedEntries == _entries) {
        return;
    }

    beginResetModel();
    _entries = std::move(synchronizedEntries);
    endResetModel();
}

} // namespace KDC
