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

#include "app/mainwindow/conflictlistmodel.h"
#include "app/navigation/activitiesnavigation.h"
#include "libcommon/utility/types.h"

#include <QObject>
#include <QString>

#include <cstdint>
#include <vector>

namespace KDC {

class ConflictVersionController;
class MainSelectionStore;
class SyncErrorService;
class SyncErrorsProjection;

/**
 * QML-facing state of the version conflicts pages: the grouped choice (`ConflictsQuick`) and the list of conflicts
 * (`ConflictsList`).
 *
 * It sends the grouped choice for every conflict of the selected synchronization, and opens the version selection dialog
 * from the list. It owns the navigation rules of these pages: after a grouped choice or once no conflict is left, back to
 * the errors page when other errors remain, to the activity list otherwise; back to the errors page when the selected
 * synchronization changes.
 */
class ConflictsPageController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(ConflictListModel *list READ list CONSTANT)
        Q_PROPERTY(int32_t count READ count NOTIFY changed)
        Q_PROPERTY(KDC::ConflictsPageController::Strategy strategy READ strategy NOTIFY changed)
        Q_PROPERTY(bool resolutionPending READ resolutionPending NOTIFY changed)
        Q_PROPERTY(bool failed READ failed NOTIFY changed)

    public:
        enum class Strategy : uint8_t {
            MostRecent,
            Online,
            Local,
        };
        Q_ENUM(Strategy)

        ConflictsPageController(const SyncErrorsProjection &projection, const MainSelectionStore &selectionStore,
                                SyncErrorService &syncErrorService, ConflictVersionController &conflictVersion,
                                ActivitiesNavigation &activitiesNavigation, QObject *parent = nullptr);

        [[nodiscard]] ConflictListModel *list() { return &_list; }
        /** Number of conflicts of the selected synchronization, whatever the search filter. */
        [[nodiscard]] int32_t count() const { return _count; }
        [[nodiscard]] Strategy strategy() const { return _strategy; }
        /** Whether a resolution of the conflicts of the selected synchronization is running, from any page. */
        [[nodiscard]] bool resolutionPending() const;
        /** Whether the last grouped choice could not be applied. */
        [[nodiscard]] bool failed() const { return _failed; }

        Q_INVOKABLE void selectStrategy(KDC::ConflictsPageController::Strategy strategy);
        /** Applies the selected strategy to every current conflict ("Apply to N files"). */
        Q_INVOKABLE void apply();
        /** Opens the list of conflicts ("Process the files individually"). */
        Q_INVOKABLE void manageIndividually();
        /** Opens the version selection dialog on every conflict, in the order of the list. */
        Q_INVOKABLE void startChoices();
        /** Opens the version selection dialog on the conflict of a row; ignored when it is gone. */
        Q_INVOKABLE void chooseVersion(const QString &rowId);

    signals:
        void changed();

    private:
        [[nodiscard]] bool isShown() const;
        [[nodiscard]] std::vector<ErrorDbId> conflictDbIds() const;
        void onPageChanged();
        void onErrorsChanged();
        void onSyncChanged();
        void showErrors(const char *reason);
        void leave(const char *reason);

        const SyncErrorsProjection &_projection;
        const MainSelectionStore &_selectionStore;
        SyncErrorService &_syncErrorService;
        ConflictVersionController &_conflictVersion;
        ActivitiesNavigation &_activitiesNavigation;
        ConflictListModel _list;
        int32_t _count{0};
        Strategy _strategy{Strategy::MostRecent};
        bool _failed{false};
        // The grouped choice sent from this page: its response, not the removal of the errors, leaves the page.
        bool _awaitingResolution{false};
        // Responses of a previous synchronization are ignored.
        uint64_t _generation{0};
        ActivitiesNavigation::Page _page{ActivitiesNavigation::Page::List};
};

} // namespace KDC
