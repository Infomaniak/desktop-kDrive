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

#include "customdialog.h"
#include "basefoldertreeitemwidget.h"

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

class ServerBaseFolderDialog : public CustomDialog {
        Q_OBJECT

        Q_PROPERTY(QColor info_icon_color READ infoIconColor WRITE setInfoIconColor)
        Q_PROPERTY(QSize info_icon_size READ infoIconSize WRITE setInfoIconSize)

    public:
        explicit ServerBaseFolderDialog(std::shared_ptr<ClientGui> gui, int accountId, const QString &localFolderName,
                                        const QString &localFolderPath, QWidget *parent = nullptr);

        inline QString serverFolderBasePath() const { return _serverFolderBasePath; }
        inline QList<QPair<QString, QString>> serverFolderList() const { return _serverFolderList; }

    private:
        std::shared_ptr<ClientGui> _gui;
        int _driveDbId;
        QString _localFolderName;
        QString _localFolderPath;

        QLabel *_infoIconLabel;
        QLabel *_availableSpaceTextLabel;
        BaseFolderTreeItemWidget *_folderTreeItemWidget;
        QPushButton *_backButton;
        QPushButton *_continueButton;
        QColor _infoIconColor;
        QSize _infoIconSize;
        bool _okToContinue;
        QString _serverFolderBasePath;
        QList<QPair<QString, QString>> _serverFolderList;

        void setButtonIcon(const QColor &value) override;

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

        void initUI();
        void updateUI();
        void setInfoIcon();
        void setOkToContinue(bool value);
        static QString remotePathTrailingSlash(const QString &path);

    private slots:
        void onExit();
        void onBackButtonTriggered(bool checked = false);
        void onContinueButtonTriggered(bool checked = false);
        void onDisplayMessage(const QString &text);
        void onFolderSelected(const QString &folderBasePath, const QList<QPair<QString, QString>> &folderList);
        void onNoFolderSelected();
};

} // namespace KDC
