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

#include "app/cache/appcache.h"
#include "app/cache/mainselectionstore.h"
#include "app/mainwindow/activitylistmodel.h"
#include "app/mainwindow/networkstatusobserver.h"
#include "app/services/activityservice.h"

#include <QObject>
#include <QString>

namespace KDC {

/**
 * QML-facing state and interaction controller for the Activities page.
 *
 * It owns the selected-sync projection, resolves page-level presentation state, validates local actions, and delegates
 * asynchronous web actions to ActivityService. It does not own recent history or active errors.
 */
class ActivitiesController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(ActivityListModel *model READ model CONSTANT)
        Q_PROPERTY(ActivityListModel::Filter filter READ filter WRITE setFilter NOTIFY filterChanged)
        Q_PROPERTY(QString title READ title NOTIFY titleChanged)
        Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
        Q_PROPERTY(bool hasActivities READ hasActivities NOTIFY hasActivitiesChanged)
        Q_PROPERTY(qint32 errorCount READ errorCount NOTIFY errorCountChanged)
        Q_PROPERTY(bool hasErrors READ hasErrors NOTIFY errorCountChanged)

    public:
        explicit ActivitiesController(const ActivityStore &activityStore, const AppCache &appCache,
                                      MainSelectionStore &selectionStore, const NetworkStatusObserver &networkStatusObserver,
                                      ActivityService &activityService, QObject *parent = nullptr);

        [[nodiscard]] ActivityListModel *model() { return &_model; }
        [[nodiscard]] ActivityListModel::Filter filter() const { return _model.filter(); }
        void setFilter(ActivityListModel::Filter filter);
        [[nodiscard]] const QString &title() const { return _title; }
        [[nodiscard]] bool loading() const { return _loading; }
        [[nodiscard]] bool hasActivities() const { return _hasActivities; }
        [[nodiscard]] qint32 errorCount() const { return _errorCount; }
        [[nodiscard]] bool hasErrors() const { return _errorCount > 0; }

        Q_INVOKABLE void openLocal(const QString &rowId);
        Q_INVOKABLE void openOnline(const QString &rowId);
        Q_INVOKABLE void copyShareLink(const QString &rowId);
        Q_INVOKABLE void requestFixErrors(const QString &rowId) const;
        Q_INVOKABLE void requestFixAllErrors() const;

    signals:
        void filterChanged();
        void titleChanged();
        void loadingChanged();
        void hasActivitiesChanged();
        void errorCountChanged();
        void shareLinkCopied(const QString &rowId);
        void actionFailed(const QString &rowId);

    private:
        void refreshPageState();
        [[nodiscard]] std::optional<SyncContext> actionSyncContext(const ActivityListModel::ActionTarget &target,
                                                                   const QString &rowId) const;

        const AppCache &_appCache;
        MainSelectionStore &_selectionStore;
        const NetworkStatusObserver &_networkStatusObserver;
        ActivityService &_activityService;
        ActivityListModel _model;
        QString _title;
        bool _loading{true};
        bool _hasActivities{false};
        qint32 _errorCount{0};
};

} // namespace KDC
