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
#include "customcheckbox.h"
#include "gui/clientgui.h"
#include "libcommon/info/exclusionappinfo.h"

#include <QTableView>
#include <QStandardItemModel>

namespace KDC {

class LiteSyncDialog : public CustomDialog {
        Q_OBJECT

        Q_PROPERTY(QColor action_icon_color READ actionIconColor WRITE setActionIconColor)
        Q_PROPERTY(QSize action_icon_size READ actionIconSize WRITE setActionIconSize)

    public:
        explicit LiteSyncDialog(std::shared_ptr<ClientGui> gui, QWidget *parent = nullptr);

    private:
        enum tableColumn {
            AppId = 0,
            AppName,
            Action
        };

        std::shared_ptr<ClientGui> _gui;

        QStandardItemModel *_appsTableModel;
        QTableView *_appsTableView;
        QPushButton *_saveButton;
        QColor _actionIconColor;
        QSize _actionIconSize;
        bool _needToSave;
        QList<ExclusionAppInfo> _defaultAppList;
        QList<ExclusionAppInfo> _userAppList;

        inline QColor actionIconColor() const { return _actionIconColor; }
        inline QSize actionIconSize() const { return _actionIconSize; }

        void initUI();
        void updateUI();
        void addApp(const ExclusionAppInfo &appInfo, bool readOnly, int &row, QString scrollToAppId, int &scrollToRow);
        void setActionIconColor(const QColor &color);
        void setActionIconSize(const QSize &size);
        void setActionIcon();
        void setActionIcon(QStandardItem *item, const QString &viewIconPath);
        void setNeedToSave(bool value);
        void loadAppTable(QString scrollToAppId = QString());

    private slots:
        void onExit();
        void onAddAppButtonTriggered(bool checked = false);
        void onTableViewClicked(const QModelIndex &index);
        void onSaveButtonTriggered(bool checked = false);
};

} // namespace KDC
