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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "app/mainwindow/synclistmodel.h"

#include <QColor>
#include <QLoggingCategory>
#include <QObject>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(lcMainSidebarController)

namespace KDC {

/**
 * QML-facing controller for main-sidebar interactions.
 *
 * It exposes the configured-sync model, delegates selection ownership to MainSelectionStore, and owns the local desktop
 * action for opening the selected synchronization folder.
 */
class MainSidebarController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QAbstractItemModel *syncsModel READ syncsModel CONSTANT)
        Q_PROPERTY(qint32 syncCount READ syncCount NOTIFY syncCountChanged)
        Q_PROPERTY(qint32 selectedRow READ selectedRow NOTIFY selectedRowChanged)
        Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY currentSyncContextChanged)
        Q_PROPERTY(QString currentSubtitle READ currentSubtitle NOTIFY currentSyncContextChanged)
        Q_PROPERTY(QColor currentDriveColor READ currentDriveColor NOTIFY currentSyncContextChanged)
        Q_PROPERTY(bool canOpenCurrentSyncFolder READ canOpenCurrentSyncFolder NOTIFY currentSyncContextChanged)
        Q_PROPERTY(qint32 currentErrorCount READ currentErrorCount NOTIFY currentSyncContextChanged)

    public:
        explicit MainSidebarController(const AppCache &cache, MainSelectionStore &selectionStore, QObject *parent = nullptr);

        [[nodiscard]] QAbstractItemModel *syncsModel() { return &_syncListModel; }
        [[nodiscard]] qint32 syncCount() const;
        [[nodiscard]] qint32 selectedRow() const;
        [[nodiscard]] QString currentTitle() const;
        [[nodiscard]] QString currentSubtitle() const;
        [[nodiscard]] QColor currentDriveColor() const;
        [[nodiscard]] bool canOpenCurrentSyncFolder() const;
        [[nodiscard]] qint32 currentErrorCount() const;

        Q_INVOKABLE void selectSync(qint64 syncDbId);
        Q_INVOKABLE bool openCurrentSyncFolder() const;

    signals:
        void currentSyncContextChanged();
        void selectedRowChanged();
        void syncCountChanged();

    private:
        [[nodiscard]] QVariant selectedData(SyncListModel::Role role) const;

        MainSelectionStore &_selectionStore;
        SyncListModel _syncListModel;
};

} // namespace KDC
