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

#include <QAbstractListModel>
#include <QColor>

#include <cstdint>
#include <vector>

namespace KDC {

struct SelectedSyncConfigurationRow {
        QString driveName;
        QColor driveColor;
        /** `~`-shortened display form of the drive local folder. */
        QString localPath;
        bool customFolder{false};
        bool customSelection{false};
};

class SelectedSyncConfigurationsModel final : public QAbstractListModel {
        Q_OBJECT

    public:
        enum Role : int32_t {
            DriveNameRole = Qt::UserRole + 1,
            DriveColorRole,
            LocalPathRole,
            CustomFolderRole,
            CustomSelectionRole,
        };
        Q_ENUM(Role)

        explicit SelectedSyncConfigurationsModel(QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
        void setRows(std::vector<SelectedSyncConfigurationRow> rows);

    private:
        std::vector<SelectedSyncConfigurationRow> _rows;
};

} // namespace KDC
