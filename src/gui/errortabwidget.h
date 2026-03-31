/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#include "customtabbar.h"
#include "gui/custompushbutton.h"

#include <QTabWidget>
#include <QListWidget>
#include <QLabel>
#include <QToolButton>
#include <QWidgetAction>

namespace KDC {

class ParametersDialog;

class ErrorTabWidget : public QTabWidget {
        Q_OBJECT

    public:
        enum ErrorTabIndex {
            ToResolveIndex = 0,
            AutoResolveIndex
        };
        explicit ErrorTabWidget(std::shared_ptr<ClientGui> gui, DriveDbId driveDbId = 0, bool generic = false,
                                QWidget *parent = nullptr);

        QListWidget *autoResolvedErrorsListWidget() const { return _autoResolvedErrorsListWidget; }
        QListWidget *unresolvedErrorsListWidget() const { return _unresolvedErrorsListWidget; }
        qint64 getLastErrorTimestamp() const { return _lastErrorTimestamp; }
        void setLastErrorTimestamp(qint64 newLastErrorTimestamp) { _lastErrorTimestamp = newLastErrorTimestamp; }
        void setDriveDbId(const DriveDbId driveDbId) { _driveDbId = driveDbId; }

        void setUnresolvedErrorsCount(const Count count);
        void setAutoResolvedErrorsCount(const Count count);
        void retranslateUi();

        void showResolveConflicts(bool val);
        void showResolveUnsupportedCharacters(bool val);

    private slots:
        void onClearAutoResErrorsClicked();
        void onClearToResErrorsClicked();
        void onScrollBarValueChanged(int value);
        void onResolveConflictErrors();
        void onResolveUnsupportedCharactersErrors();

    private:
        std::shared_ptr<ClientGui> _gui;

        CustomTabBar *_tabBar{nullptr};
        QLabel *_toResolveErrorsLabel{nullptr};
        QLabel *_toResolveErrorsNb{nullptr};
        QLabel *_autoResolvedErrorsLabel{nullptr};
        QLabel *_autoResolvedErrorsNb{nullptr};
        ParametersDialog *_paramsDialog{nullptr};

        CustomPushButton *_clearToResButton{nullptr};
        CustomPushButton *_clearAutoResButton{nullptr};
        QPushButton *_resolveButton = nullptr;
        QAction *_resolveConflictsAction = nullptr;
        QAction *_resolveUnsupportedCharactersAction = nullptr;

        QListWidget *_autoResolvedErrorsListWidget{nullptr};
        QListWidget *_unresolvedErrorsListWidget{nullptr};
        qint64 _lastErrorTimestamp = 0;
        DriveDbId _driveDbId{0};
        bool _generic{false};

        void showEvent(QShowEvent *) override;
};

} // namespace KDC
