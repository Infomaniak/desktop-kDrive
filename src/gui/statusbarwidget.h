/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "halfroundrectwidget.h"
#include "customtoolbutton.h"
#include "guiutility.h"
#include "libcommon/theme/theme.h"
#include "libcommon/info/driveinfo.h"
#include "menuwidget.h"

#include <QLabel>
#include <QWidget>

namespace KDC {

class ClientGui;

class StatusBarWidget : public HalfRoundRectWidget {
        Q_OBJECT

    public:
        explicit StatusBarWidget(std::shared_ptr<ClientGui> gui, QWidget *parent = nullptr);

        void setStatus(KDC::GuiUtility::StatusInfo &statusInfo);
        void setCurrentDrive(int driveDbId);
        void setSeveralSyncs(bool severalSyncs);
        void reset();

    signals:
        void pauseSync(ActionTarget type, int folderId = 0);
        void resumeSync(ActionTarget type, int folderId = 0);
        void linkActivated(const QString &link);

    private:
        std::shared_ptr<ClientGui> _gui;
        int _driveDbId;
        bool _severalSyncs;
        QLabel *_statusIconLabel;
        QLabel *_statusLabel;
        CustomToolButton *_pauseButton;
        CustomToolButton *_resumeButton;
        KDC::GuiUtility::StatusInfo _statusInfo;

        void onLinkActivated(const QString &link);
        void createStatusActionMenu(MenuWidget *&menu, bool &resetButtons, bool pauseClicked);
        void showStatusMenu(bool pauseClicked);
        void showEvent(QShowEvent *event) override;

        void retranslateUi();

    private slots:
        void onPauseClicked();
        void onResumeClicked();
        void onPauseSync();
        void onPauseFolderSync();
        void onPauseAllSync();
        void onResumeSync();
        void onResumeFolderSync();
        void onResumeAllSync();
};

}  // namespace KDC
