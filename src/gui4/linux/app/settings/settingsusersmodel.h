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
#include "app/settings/userdrivesmodel.h"

#include <QAbstractListModel>
#include <QString>

#include <memory>
#include <vector>

namespace KDC {

class SettingsUserService;

/** QML projection with one user card per cached UserDisplayInfo. */
class SettingsUsersModel final : public QAbstractListModel {
        Q_OBJECT
        Q_PROPERTY(int count READ count NOTIFY countChanged)

    public:
        enum Role {
            UserDbIdRole = Qt::UserRole + 1,
            NameRole,
            EmailRole,
            AvatarSourceRole,
            DrivesModelRole,
            AvailableDrivesLoadingRole,
            AvailableDrivesFailedRole,
        };
        Q_ENUM(Role)

        SettingsUsersModel(const AppCache &cache, SettingsUserService &settingsUserService, QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
        [[nodiscard]] int count() const { return rowCount(); }

    signals:
        void countChanged();

    private:
        struct UserEntry {
                UserDbId userDbId{0};
                QString name;
                QString email;
                QString avatarSource;
                std::unique_ptr<UserDrivesModel> drivesModel;
        };

        void rebuild();
        void handleUserStateChanged(UserDbId userDbId);

        const AppCache &_cache;
        SettingsUserService &_settingsUserService;
        std::vector<UserEntry> _entries;
};

} // namespace KDC
