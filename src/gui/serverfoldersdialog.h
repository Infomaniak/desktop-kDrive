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

#include "customdialog.h"
#include "foldertreeitemwidget.h"

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

class ServerFoldersDialog : public CustomDialog {
        Q_OBJECT

    public:
        explicit ServerFoldersDialog(std::shared_ptr<ClientGui> gui, int driveDbId, const QString &serverFolderName,
                                     const QString &serverFolderNodeId, QWidget *parent = nullptr);

        qint64 selectionSize() const;
        const QSet<QString> &getBlacklist() const { return _blacklist; }
        QSet<QString> getWhiteList() const;

    private:
        std::shared_ptr<ClientGui> _gui;
        int _driveDbId;
        const QString _serverFolderName;
        const QString _serverFolderNodeId;
        FolderTreeItemWidget *_folderTreeItemWidget;
        QPushButton *_backButton;
        QPushButton *_continueButton;
        bool _needToSave;
        QSet<QString> _blacklist;

        void setButtonIcon(const QColor &value) override;

        void initUI();
        void updateUI();

        void createBlackList();

    private slots:
        void onExit();
        void onBackButtonTriggered(bool checked = false);
        void onContinueButtonTriggered(bool checked = false);
        void onSubfoldersLoaded(bool error, ExitCause exitCause, bool empty);
        void onNeedToSave();
};

} // namespace KDC
