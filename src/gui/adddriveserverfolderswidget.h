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

#include "foldertreeitemwidget.h"
#include "libcommon/info/driveavailableinfo.h"

#include <QColor>
#include <QIcon>
#include <QLabel>
#include <QNetworkReply>
#include <QPushButton>
#include <QSize>
#include <QStringList>
#include <QTreeWidget>

namespace KDC {

class ClientGui;

class AddDriveServerFoldersWidget : public QWidget {
        Q_OBJECT

        Q_PROPERTY(QColor info_icon_color READ infoIconColor WRITE setInfoIconColor)
        Q_PROPERTY(QSize info_icon_size READ infoIconSize WRITE setInfoIconSize)
        Q_PROPERTY(QColor logo_color READ logoColor WRITE setLogoColor)

    public:
        explicit AddDriveServerFoldersWidget(std::shared_ptr<ClientGui> gui, QWidget *parent = nullptr);

        void init(int userDbId, const DriveAvailableInfo &driveInfo);
        qint64 selectionSize() const;
        QSet<QString> createBlackList() const;
        QSet<QString> createWhiteList() const;
        void setButtonIcon(const QColor &value);

    signals:
        void terminated(bool next = true);

    private:
        std::shared_ptr<ClientGui> _gui;
        QLabel *_logoTextIconLabel;
        QLabel *_infoIconLabel;
        QLabel *_availableSpaceTextLabel;
        FolderTreeItemWidget *_folderTreeItemWidget;
        QPushButton *_backButton;
        QPushButton *_continueButton;
        QColor _infoIconColor;
        QSize _infoIconSize;
        QColor _logoColor;
        bool _needToSave;

        inline QColor infoIconColor() const { return _infoIconColor; }
        inline void setInfoIconColor(QColor color) {
            _infoIconColor = color;
            setInfoIcon();
        }

        inline QSize infoIconSize() const { return _infoIconSize; }
        inline void setInfoIconSize(QSize size) {
            _infoIconSize = size;
            setInfoIcon();
        }

        inline QColor logoColor() const { return _logoColor; }
        void setLogoColor(const QColor &color);

        void initUI();
        void updateUI();
        void setInfoIcon();

    private slots:
        void onSubfoldersLoaded(bool error, bool empty = false);
        void onNeedToSave();
        void onBackButtonTriggered(bool checked = false);
        void onContinueButtonTriggered(bool checked = false);
};

} // namespace KDC
