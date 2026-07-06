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
#include "app/cache/onboardingstate.h"

#include <QAbstractListModel>
#include <QString>

#include <vector>

namespace KDC {

/**
 * QML list adapter for available onboarding drives.
 *
 * It exposes drive rows and stores row selection through OnboardingState. Screen state and actions belong to
 * DriveSelectionController.
 */
class AvailableDrivesModel final : public QAbstractListModel {
        Q_OBJECT

    public:
        enum Role : uint16_t {
            NameRole = Qt::UserRole + 1,
            AccountNameRole,
            ColorRole,
            SelectedRole,
            AlreadyConfiguredRole,
            EnabledRole,
            TooltipRole,
        };
        Q_ENUM(Role)

        explicit AvailableDrivesModel(AppCache &cache, OnboardingState &onboardingState, QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        [[nodiscard]] qint32 selectedCount() const;
        [[nodiscard]] qint32 configuredCount() const;

        Q_INVOKABLE void toggleDrive(qint32 row);

    signals:
        void selectedCountChanged();
        void configuredCountChanged();

    private:
        [[nodiscard]] AvailableDriveKey keyAt(qint32 row) const;
        [[nodiscard]] QString accountNameForContext(const AvailableDriveContext &context) const;
        [[nodiscard]] bool isRowSelected(qint32 row) const;
        [[nodiscard]] UserDbId selectedUserDbId() const;
        void rebuild();

        AppCache &_cache;
        OnboardingState &_onboardingState;
        std::vector<AvailableDriveContext> _contexts;
};

} // namespace KDC
