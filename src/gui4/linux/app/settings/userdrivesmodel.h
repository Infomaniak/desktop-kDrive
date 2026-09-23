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

#include <QAbstractListModel>
#include <QColor>
#include <QString>

#include <vector>

namespace KDC {

/** Cache-backed drive rows displayed under one Settings user card. */
class UserDrivesModel final : public QAbstractListModel {
        Q_OBJECT

    public:
        enum Role {
            NameRole = Qt::UserRole + 1,
            AccountNameRole,
            ColorRole,
            SynchronizedRole,
        };
        Q_ENUM(Role)

        UserDrivesModel(const AppCache &cache, UserDbId userDbId, QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    private:
        struct DriveEntry {
                QString name;
                QString accountName;
                QColor color;
                bool synchronized{false};

                friend bool operator==(const DriveEntry &lhs, const DriveEntry &rhs) = default;
        };

        void rebuild();

        const AppCache &_cache;
        UserDbId _userDbId{0};
        std::vector<DriveEntry> _entries;
};

} // namespace KDC
