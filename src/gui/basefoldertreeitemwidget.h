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

#include "libcommon/info/nodeinfo.h"

#include <QApplication>
#include <QColor>
#include <QLineEdit>
#include <QNetworkReply>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QSize>

namespace KDC {

class ClientGui;

class BaseFolderTreeItemWidget : public QTreeWidget {
        Q_OBJECT

        Q_PROPERTY(QColor folder_icon_color READ folderIconColor WRITE setFolderIconColor)
        Q_PROPERTY(QSize folder_icon_size READ folderIconSize WRITE setFolderIconSize)

        Q_PROPERTY(QColor add_icon_color READ addIconColor WRITE setAddIconColor)
        Q_PROPERTY(QSize add_icon_size READ addIconSize WRITE setAddIconSize)

    public:
        explicit BaseFolderTreeItemWidget(std::shared_ptr<ClientGui> gui, int driveDbId, bool displayRoot,
                                          QWidget *parent = nullptr);

        void loadSubFolders();

    signals:
        void message(const QString &text);
        void showMessage(bool show);
        void folderSelected(const QString &folderBasePath, const QList<QPair<QString, QString>> &folderList);
        void noFolderSelected();

    private:
        class CustomDelegate : public QStyledItemDelegate {
            public:
                CustomDelegate(BaseFolderTreeItemWidget *treeWidget, QObject *parent = nullptr);
                void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const override;

            private:
                BaseFolderTreeItemWidget *_treeWidget;
        };

        enum TreeWidgetColumn { Folder = 0, Action };

        std::shared_ptr<ClientGui> _gui;
        int _driveDbId;
        bool _displayRoot;
        QColor _folderIconColor;
        QSize _folderIconSize;
        QColor _addIconColor;
        QSize _addIconSize;
        bool _inserting;

        inline QColor folderIconColor() const { return _folderIconColor; }
        inline void setFolderIconColor(QColor color) {
            _folderIconColor = color;
            setFolderIcon();
        }

        inline QSize folderIconSize() const { return _folderIconSize; }
        inline void setFolderIconSize(QSize size) {
            _folderIconSize = size;
            setFolderIcon();
        }

        inline QColor addIconColor() const { return _addIconColor; }
        inline void setAddIconColor(QColor color) { _addIconColor = color; }

        inline QSize addIconSize() const { return _addIconSize; }
        inline void setAddIconSize(QSize size) { _addIconSize = size; }

        void initUI();
        QString iconPath(const QString &folderName);
        QColor iconColor(const QString &folderName);
        void setFolderIcon();
        void setFolderIcon(QTreeWidgetItem *item, const QString &folderName);
        void setSubFoldersIcon(QTreeWidgetItem *parent);
        QTreeWidgetItem *findFirstChild(QTreeWidgetItem *parent, const QString &text);
        void insertNode(QTreeWidgetItem *parent, const NodeInfo &folderInfo);
        QList<QPair<QString, QString>> buildFolderList(QTreeWidgetItem *selectedItem);

    private slots:
        void updateDirectories(QTreeWidgetItem *item, QList<NodeInfo> list);
        void onItemExpanded(QTreeWidgetItem *item);
        void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
        void onItemClicked(QTreeWidgetItem *item, int column);
        void onItemDoubleClicked(QTreeWidgetItem *item, int column);
        void onItemChanged(QTreeWidgetItem *item, int column);
};

} // namespace KDC
