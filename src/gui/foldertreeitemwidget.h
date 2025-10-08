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

#include "customtreewidgetitem.h"
#include "libcommon/utility/types.h"
#include "libcommon/info/driveavailableinfo.h"
#include "libcommon/info/nodeinfo.h"

#include <QColor>
#include <QTreeWidget>
#include <QSize>

namespace KDC {

class ClientGui;

class FolderTreeItemWidget : public QTreeWidget {
        Q_OBJECT

    public:
        FolderTreeItemWidget(std::shared_ptr<ClientGui> gui, bool displayRoot, QWidget *parent = nullptr);

        void setSyncDbId(int syncDbId);
        void setUserDbIdAndDriveInfo(int userDbId, const DriveAvailableInfo &driveInfo);
        void setDriveDbIdAndFolderNodeId(int driveDbId, const QString &serverFolderNodeId);
        void loadSubFolders();
        QSet<QString> createBlackSet();
        QSet<QString> createWhiteSet();
        int syncDbId() const { return _syncDbId; }
        qint64 nodeSize(QTreeWidgetItem *item) const;

    signals:
        void terminated(bool error, ExitCause exitCause, bool empty = false);
        void needToSave(bool isBlackListed);

    private:
        enum TreeWidgetColumn {
            Folder = 0,
            Size
        };

        std::shared_ptr<ClientGui> _gui;
        int _syncDbId{0};
        int _userDbId{0};
        int _driveId{0};
        QString _driveName;
        QColor _driveColor;
        QString _nodeId;
        bool _displayRoot;
        QSet<QString> _oldBlackList;
        QSet<QString> _oldUndecidedList;
        static const QColor _folderIconColor;
        static const QSize _folderIconSize;
        static const QColor _sizeTextColor;
        bool _inserting{false};
        QHash<QString, QTreeWidgetItem *> _subFoldersMap;
        using GuiNodeId = QString;
        using GuiPath = QString;
        QHash<GuiNodeId, GuiPath> _blacklistCache;
        CustomTreeWidgetItem *_root = nullptr;

        // The set of items newly blacklisted by the user; they were
        // whitelisted during the latest synchronization.
        QSet<const QTreeWidgetItem *> _newlyBlackListedItems;

        void initUI();
        QString iconPath(const QString &folderName);
        QColor iconColor(const QString &folderName);
        void setFolderIcon();
        void setFolderIcon(QTreeWidgetItem *item, const QString &folderName);
        void setSubFoldersIcon(QTreeWidgetItem *parent);
        QTreeWidgetItem *findFirstChild(QTreeWidgetItem *parent, const QString &text);
        void insertNode(QTreeWidgetItem *parent, const NodeInfo &nodeInfo);
        void updateDirectories(QTreeWidgetItem *item, const QString &nodeId, QList<NodeInfo> list);
        ExitCode updateBlackUndecidedSet();
        void createBlackSet(const QTreeWidgetItem *parentItem, QSet<QString> &blackset);
        void removeChildNodeFromSet(const QString &nodeId, QSet<QString> &blackset);
        void createWhiteSet(QTreeWidgetItem *parentItem, QSet<QString> &whiteSet);
        void updateBlacklistPathMap();
        void addTreeWidgetItemToQueue(const QString &nodeId, QTreeWidgetItem *item);

        void setDriveDbId(int driveDbId);
        void setItemSize(QTreeWidgetItem *item, qint64 size);

        bool isRoot(const QTreeWidgetItem *item) const { return item == _root; }

        // Returns true if the user unchecked `item` while the corresponding
        // node is not currently blacklisted, false otherwise.
        bool isNewlyBlackListed(const QTreeWidgetItem *item) const;

        // Returns true if the item, or any of its ancestors is in the
        // checked state. Returns false otherwise.
        bool isWhiteListed(const QTreeWidgetItem *item) const;

        // Update the set of newly blacklisted items wrt to the latest synchronization
        // depending on user actions (check or uncheck folder items).
        void updateNewlyBlackListedItems(const QTreeWidgetItem *item);

        // Returns the relative path associated of the item identified with remote identifier `nodeId`
        // Note: We first check the existence of the path in _blacklistCache first to avoid unnecessary HTTP GET requests.
        QString getPath(const QString &nodeId);


    private slots:
        void onItemExpanded(QTreeWidgetItem *item);
        void onItemChanged(QTreeWidgetItem *item, int col);
        void onSyncListRefreshed();
        void onFolderSizeCompleted(const QString &nodeId, qint64 size);
};

} // namespace KDC
