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

#include "foldertreeitemwidget.h"
#include "guiutility.h"
#include "guirequests.h"
#include "clientgui.h"
#include "libcommongui/matomoclient.h"
#include "libcommongui/utility/utility.h"

#include <QDir>
#include <QHeaderView>
#include <QMutableListIterator>
#include <QScopedValueRollback>
#include <QLoggingCategory>

namespace KDC {

static const int treeWidgetIndentation = 30;

static const QString commonDocumentsFolderName("Common documents");
static const QString sharedFolderName("Shared");

// 1st column roles
static const int viewIconPathRole = Qt::UserRole;
static const int nodeIdRole = Qt::UserRole + 1;

// 2nd column roles
static const int sizeRole = Qt::UserRole;

const QColor FolderTreeItemWidget::_folderIconColor = QColor(0x9F9F9F);
const QSize FolderTreeItemWidget::_folderIconSize = QSize(20, 20);
const QColor FolderTreeItemWidget::_sizeTextColor = QColor(0x939393);

Q_LOGGING_CATEGORY(lcFolderTreeItemWidget, "gui.foldertreeitemwidget", QtInfoMsg)

FolderTreeItemWidget::FolderTreeItemWidget(std::shared_ptr<ClientGui> gui, bool displayRoot, QWidget *parent) :
    QTreeWidget(parent),
    _gui(gui),
    _displayRoot(displayRoot) {
    initUI();

    // Make sure we don't get crashes if the sync is destroyed while the dialog is still open.
    connect(_gui.get(), &ClientGui::syncListRefreshed, this, &FolderTreeItemWidget::onSyncListRefreshed);
    connect(_gui.get(), &ClientGui::folderSizeCompleted, this, &FolderTreeItemWidget::onFolderSizeCompleted);
}

void FolderTreeItemWidget::setSyncDbId(int syncDbId) {
    _syncDbId = syncDbId;

    if (const auto exitCode = updateBlackUndecidedSet(); exitCode != ExitCode::Ok) {
        qCWarning(lcFolderTreeItemWidget()) << "Error in updateBlackUndecidedSet";
        return;
    }

    if (const auto exitCode = GuiRequests::getDriveIdFromSyncDbId(_syncDbId, _driveId); exitCode != ExitCode::Ok) {
        qCWarning(lcFolderTreeItemWidget()) << "Error in GuiRequests::getDriveIdFromSyncDbId";
        return;
    }

    const auto &syncInfoMapIt = _gui->syncInfoMap().find(_syncDbId);
    if (syncInfoMapIt == _gui->syncInfoMap().end()) {
        qCWarning(lcFolderTreeItemWidget()) << "Sync not found in sync map for syncDbId=" << _syncDbId;
        return;
    }

    _nodeId = syncInfoMapIt->second.targetNodeId();

    setDriveDbId(syncInfoMapIt->second.driveDbId());
}

void FolderTreeItemWidget::setDriveDbId(int driveDbId) {
    const auto &driveInfoMapIt = _gui->driveInfoMap().find(driveDbId);
    if (driveInfoMapIt == _gui->driveInfoMap().end()) {
        qCWarning(lcFolderTreeItemWidget()) << "Drive not found in drive map for driveDbId=" << driveDbId;
        return;
    }

    const auto &accountInfoMapIt = _gui->accountInfoMap().find(driveInfoMapIt->second.accountDbId());
    if (accountInfoMapIt == _gui->accountInfoMap().end()) {
        qCWarning(lcFolderTreeItemWidget()) << "Account not found in account map for accountDbId="
                                            << driveInfoMapIt->second.accountDbId();
        return;
    }

    _driveName = driveInfoMapIt->second.name();
    _driveColor = driveInfoMapIt->second.color();
    _userDbId = accountInfoMapIt->second.userDbId();
}

void FolderTreeItemWidget::setUserDbIdAndDriveInfo(int userDbId, const DriveAvailableInfo &driveInfo) {
    _userDbId = userDbId;
    _driveId = driveInfo.driveId();
    _driveName = driveInfo.name();
    _driveColor = driveInfo.color();
}

void FolderTreeItemWidget::setDriveDbIdAndFolderNodeId(int driveDbId, const QString &serverFolderNodeId) {
    if (const auto exitCode = GuiRequests::getDriveIdFromDriveDbId(driveDbId, _driveId); ExitCode::Ok != exitCode) {
        qCWarning(lcFolderTreeItemWidget()) << "Error in GuiRequests::getDriveIdFromDriveDbId";
        return;
    }

    _nodeId = serverFolderNodeId;

    setDriveDbId(driveDbId);
}

void FolderTreeItemWidget::loadSubFolders() {
    if (!_userDbId || !_driveId) return;

    _subFoldersMap.clear();
    _blacklistCache.clear();
    clear();

    QList<NodeInfo> nodeInfoList;
    if (const auto exitInfo = GuiRequests::getSubFolders(_userDbId, _driveId, _nodeId, nodeInfoList, true); !exitInfo) {
        qCWarning(lcFolderTreeItemWidget()) << "Error in GuiRequests::getSubFolders";
        emit terminated(true, exitInfo.cause());
        return;
    }

    if (const auto exitCode = updateBlackUndecidedSet(); exitCode != ExitCode::Ok) {
        qCWarning(lcFolderTreeItemWidget()) << "Error in updateBlackUndecidedSet";
        return;
    }

    // Keep blacklisted nodes path in cache
    updateBlacklistPathMap();

    if (!nodeInfoList.empty()) {
        updateDirectories(nullptr, _nodeId, nodeInfoList);
    } else {
        // No sub folders
        emit terminated(false, ExitCause::Unknown, true);
    }
}

ExitCode FolderTreeItemWidget::updateBlackUndecidedSet() {
    _newlyBlackListedItems.clear();

    if (!_syncDbId) return ExitCode::Ok;

    bool userConnected = false;
    if (const auto userInfoIt = _gui->userInfoMap().find(_userDbId); userInfoIt != _gui->userInfoMap().end()) {
        userConnected = userInfoIt->second.connected();
    }

    if (!userConnected) return ExitCode::Ok;

    if (const auto exitCode = GuiRequests::getSyncIdSet(_syncDbId, SyncNodeType::BlackList, _oldBlackList);
        exitCode != ExitCode::Ok) {
        qCWarning(lcFolderTreeItemWidget()) << "Error in GuiRequests::getSyncIdSet with SyncNodeType::BlackList";
        return exitCode;
    }

    if (const auto exitCode = GuiRequests::getSyncIdSet(_syncDbId, SyncNodeType::UndecidedList, _oldUndecidedList);
        exitCode != ExitCode::Ok) {
        qCWarning(lcFolderTreeItemWidget()) << "Error in GuiRequests::getSyncIdSet with SyncNodeType::UndecidedList";
        return exitCode;
    }

    return ExitCode::Ok;
}

void FolderTreeItemWidget::updateBlacklistPathMap() {
    for (auto i = 0; i < 2; ++i) {
        for (const QString &nodeId: i == 0 ? _oldBlackList : _oldUndecidedList) {
            QString path;
            if (const auto exitCode = GuiRequests::getNodePath(_syncDbId, nodeId, path); exitCode != ExitCode::Ok) {
                qCWarning(lcFolderTreeItemWidget()) << "Error in GuiRequests::getNodePath";
                continue;
            }

            (void) _blacklistCache.insert(nodeId, path);
        }
    }
}

void FolderTreeItemWidget::insertNode(QTreeWidgetItem *parent, const NodeInfo &nodeInfo) {
    auto *item = dynamic_cast<CustomTreeWidgetItem *>(findFirstChild(parent, nodeInfo.name()));
    if (item) return;

    item = new CustomTreeWidgetItem(parent);
    item->setData(TreeWidgetColumn::Folder, nodeIdRole, nodeInfo.nodeId());

    // Set check status
    if (const bool ignoreState = !_displayRoot && isRoot(parent);
        !ignoreState && parent->checkState(TreeWidgetColumn::Folder) == Qt::Checked) {
        item->setCheckState(TreeWidgetColumn::Folder, Qt::Checked);
    } else if (!ignoreState && parent->checkState(TreeWidgetColumn::Folder) == Qt::Unchecked) {
        item->setCheckState(TreeWidgetColumn::Folder, Qt::Unchecked);
    } else {
        if (_oldBlackList.contains(nodeInfo.nodeId()) || _oldUndecidedList.contains(nodeInfo.nodeId())) {
            // Current item is blacklisted
            item->setCheckState(TreeWidgetColumn::Folder, Qt::Unchecked);
        } else {
            bool isChecked = true;
            foreach (const QString &blacklistItemPath, _blacklistCache) {
                if (blacklistItemPath.startsWith(nodeInfo.path() + "/")) {
                    // At least 1 child is blacklisted
                    item->setCheckState(TreeWidgetColumn::Folder, Qt::PartiallyChecked);
                    isChecked = false;
                    break;
                }
            }

            if (isChecked) {
                item->setCheckState(TreeWidgetColumn::Folder, Qt::Checked);
            }
        }
    }

    // Set icon
    setFolderIcon(item, nodeInfo.name());

    // Set name
    item->setText(TreeWidgetColumn::Folder, nodeInfo.name());

    if (!nodeInfo.nodeId().isEmpty()) {
        addTreeWidgetItemToQueue(nodeInfo.nodeId(), item);
    }

    item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
}

QSet<QString> FolderTreeItemWidget::createBlackSet() {
    QSet<QString> newBlackSet = _oldBlackList.unite(_oldUndecidedList);
    createBlackSet(nullptr, newBlackSet);
    return newBlackSet;
}

QString FolderTreeItemWidget::getPath(const QString &nodeId) {
    QString path;

    if (_blacklistCache.contains(nodeId)) {
        path = _blacklistCache[nodeId];
    } else if (const auto exitCode = GuiRequests::getNodePath(_syncDbId, nodeId, path); exitCode != ExitCode::Ok) {
        qCWarning(lcFolderTreeItemWidget()) << "Error in GuiRequests::getNodePath. Failed to retrieve path of nodeId '" << nodeId
                                            << "'.";
    }

    return path;
}

void FolderTreeItemWidget::createBlackSet(const QTreeWidgetItem *parentItem, QSet<QString> &blackset) {
    if (!parentItem) parentItem = topLevelItem(0);
    if (!parentItem) return;

    const auto parentNodeId = parentItem->data(TreeWidgetColumn::Folder, nodeIdRole).toString();

    switch (parentItem->checkState(TreeWidgetColumn::Folder)) {
        case Qt::Unchecked: {
            const QString path = getPath(parentNodeId);
            if (!path.isEmpty()) {
                (void) blackset.insert(parentNodeId);
                (void) _blacklistCache.emplace(parentNodeId, path);
            }
            removeChildNodeFromSet(path, blackset);
            return;
        }
        case Qt::Checked: {
            const QString path = getPath(parentNodeId);
            removeChildNodeFromSet(path, blackset);
            break;
        }
        case Qt::PartiallyChecked:
            break;
    }

    (void) blackset.remove(parentNodeId);
    (void) _blacklistCache.remove(parentNodeId);

    for (auto i = 0; i < parentItem->childCount(); ++i) createBlackSet(parentItem->child(i), blackset);
}

void FolderTreeItemWidget::removeChildNodeFromSet(const QString &parentPath, QSet<QString> &blackset) {
    if (parentPath.isEmpty()) return;

    for (auto it = _blacklistCache.begin(); it != _blacklistCache.end();) {
        if (const auto &path = it.value(); path.startsWith(parentPath + "/")) {
            (void) blackset.remove(it.key());
            it = _blacklistCache.erase(it);
            continue;
        }
        ++it;
    }
}

QSet<QString> FolderTreeItemWidget::createWhiteSet() {
    QSet<QString> newWhiteSet;
    createWhiteSet(nullptr, newWhiteSet);
    return newWhiteSet;
}

void FolderTreeItemWidget::createWhiteSet(QTreeWidgetItem *parentItem, QSet<QString> &whiteSet) {
    if (!parentItem) parentItem = topLevelItem(0);

    if (!parentItem) return;

    QString parentNodeId = parentItem->data(TreeWidgetColumn::Folder, nodeIdRole).toString();

    switch (parentItem->checkState(TreeWidgetColumn::Folder)) {
        case Qt::Unchecked: {
            return;
        }
        case Qt::Checked: {
            whiteSet.insert(parentNodeId);
            return;
        }
        case Qt::PartiallyChecked: {
            whiteSet.insert(parentNodeId);
            break;
        }
    }

    for (auto i = 0; i < parentItem->childCount(); ++i) createWhiteSet(parentItem->child(i), whiteSet);
}

qint64 FolderTreeItemWidget::nodeSize(QTreeWidgetItem *item) const {
    qint64 size = 0;
    if (item->checkState(TreeWidgetColumn::Folder) == Qt::Checked) {
        size = item->data(TreeWidgetColumn::Size, sizeRole).toLongLong();
    } else if (item->checkState(TreeWidgetColumn::Folder) == Qt::PartiallyChecked) {
        size = item->data(TreeWidgetColumn::Size, sizeRole).toLongLong();

        // Remove the size of unchecked subfolders
        for (auto i = 0; i < item->childCount(); ++i) {
            if (item->child(i)->checkState(TreeWidgetColumn::Folder) == Qt::PartiallyChecked) {
                size -= nodeSize(item->child(i));
            } else if (item->child(i)->checkState(TreeWidgetColumn::Folder) == Qt::Unchecked) {
                size -= item->child(i)->data(TreeWidgetColumn::Size, sizeRole).toLongLong();
            }
        }
    }
    return size;
}

void FolderTreeItemWidget::initUI() {
    setSelectionMode(QAbstractItemView::NoSelection);
    setSortingEnabled(true);
    sortByColumn(TreeWidgetColumn::Folder, Qt::AscendingOrder);
    setColumnCount(2);
    header()->hide();
    header()->setSectionResizeMode(TreeWidgetColumn::Folder, QHeaderView::Stretch);
    header()->setSectionResizeMode(TreeWidgetColumn::Size, QHeaderView::ResizeToContents);
    header()->setStretchLastSection(false);
    setIndentation(treeWidgetIndentation);
    setRootIsDecorated(!_displayRoot);
    setFolderIcon();

    connect(this, &QTreeWidget::itemExpanded, this, &FolderTreeItemWidget::onItemExpanded);
    connect(this, &QTreeWidget::itemChanged, this, &FolderTreeItemWidget::onItemChanged);
}

QString FolderTreeItemWidget::iconPath(const QString &folderName) {
    QString iconPath;
    if (folderName == commonDocumentsFolderName) {
        iconPath = ":/client/resources/icons/document types/folder-common-documents.svg";
    } else if (folderName == sharedFolderName) {
        iconPath = ":/client/resources/icons/document types/folder-disable.svg";
    } else {
        iconPath = ":/client/resources/icons/actions/folder.svg";
    }
    return iconPath;
}

QColor FolderTreeItemWidget::iconColor(const QString &folderName) {
    QColor iconColor;
    if (folderName == commonDocumentsFolderName) {
        iconColor = QColor();
    } else if (folderName == sharedFolderName) {
        iconColor = QColor();
    } else {
        iconColor = _folderIconColor;
    }
    return iconColor;
}

void FolderTreeItemWidget::setFolderIcon() {
    if (_folderIconColor != QColor() && _folderIconSize != QSize()) {
        setIconSize(_folderIconSize);
        if (topLevelItem(0)) {
            setSubFoldersIcon(topLevelItem(0));
        }
    }
}

void FolderTreeItemWidget::setFolderIcon(QTreeWidgetItem *item, const QString &folderName) {
    if (item) {
        if (item->data(TreeWidgetColumn::Folder, viewIconPathRole).isNull()) {
            item->setData(TreeWidgetColumn::Folder, viewIconPathRole, iconPath(folderName));
        }
        if (_folderIconColor != QColor() && _folderIconSize != QSize()) {
            item->setIcon(TreeWidgetColumn::Folder,
                          KDC::GuiUtility::getIconWithColor(iconPath(folderName), iconColor(folderName)));
        }
    }
}

void FolderTreeItemWidget::setSubFoldersIcon(QTreeWidgetItem *parent) {
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *item = parent->child(i);
        QVariant viewIconPathV = item->data(TreeWidgetColumn::Folder, viewIconPathRole);
        if (!viewIconPathV.isNull()) {
            QString viewIconPath = qvariant_cast<QString>(viewIconPathV);
            setFolderIcon(item, viewIconPath);
        }
        setSubFoldersIcon(item);
    }
}

QTreeWidgetItem *FolderTreeItemWidget::findFirstChild(QTreeWidgetItem *parent, const QString &text) {
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);
        if (child->text(TreeWidgetColumn::Folder) == text) {
            return child;
        }
    }
    return 0;
}

void FolderTreeItemWidget::updateDirectories(QTreeWidgetItem *item, const QString &nodeId, QList<NodeInfo> list) {
    QScopedValueRollback<bool> isInserting(_inserting);
    _inserting = true;

    // Check for excludes
    QMutableListIterator<NodeInfo> it(list);
    while (it.hasNext()) {
        bool excluded = false;
        if (const auto exitCode = GuiRequests::getNameExcluded(it.next().name(), excluded); ExitCode::Ok != exitCode) {
            qCWarning(lcFolderTreeItemWidget()) << "Error in GuiRequests::getNameExcluded";
            return;
        }

        if (excluded) it.remove();
    }

    if (list.empty()) {
        // No sub folder
        emit terminated(false, ExitCause::Unknown, true);
        return;
    }

    emit terminated(false, ExitCause::Unknown, false);

    _root = dynamic_cast<CustomTreeWidgetItem *>(topLevelItem(0));
    if (!_root) {
        // Create root node
        _root = new CustomTreeWidgetItem(this);
        if (!_displayRoot) {
            setRootIndex(indexFromItem(_root));
        }
        if (nodeId.isEmpty()) {
            // Drive root
            _root->setText(TreeWidgetColumn::Folder, _driveName);
            _root->setIcon(TreeWidgetColumn::Folder,
                           KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/drive.svg", _driveColor));
        } else {
            NodeInfo nodeInfo;
            if (const auto exitCode = GuiRequests::getNodeInfo(_userDbId, _driveId, nodeId, nodeInfo); exitCode != ExitCode::Ok) {
                qCWarning(lcFolderTreeItemWidget()) << "Error in GuiRequests::getNodeInfo";
                return;
            }

            _root->setText(TreeWidgetColumn::Folder, nodeInfo.name());
            setFolderIcon(_root, ":/client/resources/icons/actions/folder.svg");

            // Set size
            addTreeWidgetItemToQueue(nodeId, _root);

            _root->setData(TreeWidgetColumn::Folder, nodeIdRole, nodeId);
        }

        // Set check state
        _root->setCheckState(TreeWidgetColumn::Folder, Qt::Checked);
    }

    if (!item) {
        item = _root;
    }

    KDC::CommonGuiUtility::sortSubfolders(list);
    for (const NodeInfo &nodeInfo: list) {
        insertNode(item, nodeInfo);
    }

    // Root is partially checked if any children are not checked
    for (auto i = 0; i < _root->childCount(); ++i) {
        const auto child = _root->child(i);
        if (child->checkState(TreeWidgetColumn::Folder) != Qt::Checked) {
            _root->setCheckState(TreeWidgetColumn::Folder, Qt::PartiallyChecked);
            break;
        }
    }

    _root->setExpanded(true);
}

void FolderTreeItemWidget::onItemExpanded(QTreeWidgetItem *item) {
    MatomoClient::sendEvent("folderTreeItem", MatomoEventAction::Click, "expandButton");
    item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicatorWhenChildless);

    const QString nodeId = item->data(TreeWidgetColumn::Folder, nodeIdRole).toString();
    if (nodeId.isEmpty()) {
        return;
    }

    QList<NodeInfo> nodeInfoList;
    if (const auto exitInfo = GuiRequests::getSubFolders(_userDbId, _driveId, nodeId, nodeInfoList, true); !exitInfo) {
        qCWarning(lcFolderTreeItemWidget()) << "Error in GuiRequests::getSubFolders";
        return;
    }

    updateDirectories(item, nodeId, nodeInfoList);
}

bool FolderTreeItemWidget::isNewlyBlackListed(const QTreeWidgetItem *item) const {
    if (const bool isUnchecked = item->checkState(TreeWidgetColumn::Folder) == Qt::Unchecked; !isUnchecked) {
        return false;
    }

    // The descendants of a blacklisted item are only implicitly blacklisted:
    // such descendants are not stored in `_oldBlackList` or `_oldUndecidedList`.
    // Therefore, ancestors need to be checked.
    while (item != nullptr) {
        const QString id = item->data(TreeWidgetColumn::Folder, nodeIdRole).toString();
        if (_oldBlackList.contains(id) || _oldUndecidedList.contains(id)) {
            break;
        }
        item = item->parent();
    }

    return item == nullptr;
}

bool FolderTreeItemWidget::isWhiteListed(const QTreeWidgetItem *item) const {
    while (item != nullptr) {
        if (item->checkState(TreeWidgetColumn::Folder) == Qt::Checked) {
            return true;
        }
        item = item->parent();
    }

    return false;
}

void FolderTreeItemWidget::updateNewlyBlackListedItems(const QTreeWidgetItem *item) {
    if (isNewlyBlackListed(item)) {
        _newlyBlackListedItems.insert(item);
        return;
    }

    if (item->checkState(TreeWidgetColumn::Folder) == Qt::Checked) {
        for (auto it = _newlyBlackListedItems.begin(); it != _newlyBlackListedItems.end();) {
            if (isWhiteListed(*it)) {
                it = _newlyBlackListedItems.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void FolderTreeItemWidget::onItemChanged(QTreeWidgetItem *item, int col) {
    if (col != TreeWidgetColumn::Folder || _inserting) {
        return;
    }

    const Qt::CheckState checkState = item->checkState(TreeWidgetColumn::Folder);
    QTreeWidgetItem *const parent = item->parent();
    if (checkState != Qt::PartiallyChecked) {
        MatomoClient::sendEvent("folderTreeItem", MatomoEventAction::Click, "syncFolder", (checkState == Qt::Checked ? 1 : 0));
    }
    switch (checkState) {
        case Qt::Checked: {
            // Need to check the parent as well if all siblings are also checked
            if (parent && parent->checkState(TreeWidgetColumn::Folder) != Qt::Checked) {
                bool hasUnchecked = false;
                for (auto i = 0; i < parent->childCount(); ++i) {
                    if (parent->child(i)->checkState(TreeWidgetColumn::Folder) != Qt::Checked) {
                        hasUnchecked = true;
                        break;
                    }
                }
                if (!hasUnchecked) {
                    parent->setCheckState(TreeWidgetColumn::Folder, Qt::Checked);
                } else if (parent->checkState(TreeWidgetColumn::Folder) == Qt::Unchecked) {
                    parent->setCheckState(TreeWidgetColumn::Folder, Qt::PartiallyChecked);
                }
            }

            // Check all children
            for (auto i = 0; i < item->childCount(); ++i) {
                item->child(i)->setCheckState(TreeWidgetColumn::Folder, Qt::Checked);
            }
        } break;
        case Qt::Unchecked: {
            if (parent && parent->checkState(TreeWidgetColumn::Folder) == Qt::Checked) {
                parent->setCheckState(TreeWidgetColumn::Folder, Qt::PartiallyChecked);
            }

            // Uncheck all children
            for (int i = 0; i < item->childCount(); ++i) {
                item->child(i)->setCheckState(TreeWidgetColumn::Folder, Qt::Unchecked);
            }

            // Can't uncheck the root.
            if (!parent) {
                item->setCheckState(TreeWidgetColumn::Folder, Qt::PartiallyChecked);
            }
        } break;
        case Qt::PartiallyChecked: {
            if (parent) {
                parent->setCheckState(TreeWidgetColumn::Folder, Qt::PartiallyChecked);
            }
        } break;
    }

    if (checkState != Qt::PartiallyChecked) {
        updateNewlyBlackListedItems(item);
    }
    emit needToSave(!_newlyBlackListedItems.empty());
}

void FolderTreeItemWidget::onSyncListRefreshed() {
    if (_syncDbId && _gui->syncInfoMap().find(_syncDbId) == _gui->syncInfoMap().end()) {
        deleteLater();
    }
}

void FolderTreeItemWidget::onFolderSizeCompleted(const QString &nodeId, const qint64 size) {
    const auto &subFoldersMapIt = _subFoldersMap.find(nodeId);
    if (subFoldersMapIt != _subFoldersMap.end()) {
        setItemSize(subFoldersMapIt.value(), size);
        _subFoldersMap.erase(subFoldersMapIt);
    }
}

void FolderTreeItemWidget::setItemSize(QTreeWidgetItem *item, qint64 size) {
    if (size >= 0) {
        item->setText(TreeWidgetColumn::Size, KDC::CommonGuiUtility::octetsToString(size));
        item->setData(TreeWidgetColumn::Size, sizeRole, size);
    } else {
        item->setText(TreeWidgetColumn::Size, QString());
    }
    item->setForeground(TreeWidgetColumn::Size, _sizeTextColor);
    item->setTextAlignment(TreeWidgetColumn::Size, Qt::AlignRight | Qt::AlignVCenter);
}

void FolderTreeItemWidget::addTreeWidgetItemToQueue(const QString &nodeId, QTreeWidgetItem *item) {
    _subFoldersMap[nodeId] = item;

    // Get new size
    ExitCode exitCode = GuiRequests::getFolderSize(_userDbId, _driveId, nodeId);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcFolderTreeItemWidget()) << "Error in GuiRequests::getFolderSize for userDbId=" << _userDbId
                                            << " driveId=" << _driveId << " nodeId=" << nodeId;
    }
}

} // namespace KDC
