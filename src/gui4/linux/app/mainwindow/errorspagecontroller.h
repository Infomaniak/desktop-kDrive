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

#include "app/mainwindow/errorlistmodel.h"

#include <QObject>
#include <QString>

#include <cstdint>
#include <vector>

namespace KDC {

class ActivitiesNavigation;
class MainSelectionStore;
class SyncErrorActionHandler;
class SyncErrorService;
class SyncErrorsProjection;
class SyncService;

/**
 * QML-facing state of the errors page ("Errors to fix") for the selected synchronization.
 *
 * It owns the row model, exposes the error count and the refresh state, and revalidates each row action against the
 * current errors before handing it to `SyncErrorActionHandler`: a row can disappear between its display and the click.
 */
class ErrorsPageController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(ErrorListModel *model READ model CONSTANT)
        Q_PROPERTY(int32_t errorCount READ errorCount NOTIFY errorCountChanged)
        Q_PROPERTY(bool refreshPending READ refreshPending NOTIFY refreshPendingChanged)

    public:
        ErrorsPageController(const SyncErrorsProjection &projection, const MainSelectionStore &selectionStore,
                             const SyncService &syncService, SyncErrorService &syncErrorService,
                             SyncErrorActionHandler &actionHandler, ActivitiesNavigation &activitiesNavigation,
                             QObject *parent = nullptr);

        [[nodiscard]] ErrorListModel *model() { return &_model; }
        [[nodiscard]] int32_t errorCount() const;
        [[nodiscard]] bool refreshPending() const;

        /** Asks the server to detect the errors of the selected synchronization again ("Refresh"). */
        Q_INVOKABLE void refresh();
        /** Runs the action of a row; ignored when its error is gone. */
        Q_INVOKABLE void invokeAction(const QString &rowId);
        /** Copies the absolute destination path of a row to the clipboard; the row shows its `~` display form. */
        Q_INVOKABLE void copyPath(const QString &rowId) const;
        /** Copies the technical details of a row to the clipboard, to be sent to the support. */
        Q_INVOKABLE void copyTechnicalDetails(const QString &rowId) const;
        /** Asks the page to bring the first row of `errorDbIds` into view when it is shown next ("Fix errors" of an
         * activity row). */
        void reveal(const std::vector<ErrorDbId> &errorDbIds);
        /** Returns the row to bring into view, or -1, and clears the request: a later opening shows the top of the page. */
        Q_INVOKABLE int32_t takeRevealRow();
        /** Returns to the activity list ("View activities" of the empty state). */
        Q_INVOKABLE void showActivities();

    signals:
        void errorCountChanged();
        void refreshPendingChanged();

    private:
        [[nodiscard]] const ClassifiedSyncError *currentError(const QString &rowId) const;

        const SyncErrorsProjection &_projection;
        const MainSelectionStore &_selectionStore;
        SyncErrorService &_syncErrorService;
        SyncErrorActionHandler &_actionHandler;
        ActivitiesNavigation &_activitiesNavigation;
        ErrorListModel _model;
        int32_t _errorCount{0};
        std::vector<ErrorDbId> _revealErrorDbIds;
};

} // namespace KDC
