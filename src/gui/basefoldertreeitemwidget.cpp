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

#include "guirequests.h"
#include "basefoldertreeitemwidget.h"
#include "customtreewidgetitem.h"
#include "guiutility.h"
#include "common/utility.h"
#include "libcommon/utility/utility.h"
#include "libcommongui/utility/utility.h"
#include "clientgui.h"
#include "libcommon/theme/theme.h"

#include <QDir>
#include <QHeaderView>
#include <QMutableListIterator>
#include <QScopedValueRollback>
#include <QTimer>
#include <QLoggingCategory>

namespace KDC {

static const int treeWidgetIndentation = 30;
static const int treeWidgetEditorMargin = 2;

static const QString commonDocumentsFolderName("Common documents");
static const QString sharedFolderName("Shared");

// 1st column roles
static const int viewIconPathRole = Qt::UserRole;
static const int dirRole = Qt::UserRole + 1;
static const int baseDirRole = Qt::UserRole + 2;
static const int nodeIdRole = Qt::UserRole + 3;

Q_LOGGING_CATEGORY(lcBaseFolderTreeItemWidget, "gui.foldertreeitemwidget", QtInfoMsg)

BaseFolderTreeItemWidget::BaseFolderTreeItemWidget(std::shared_ptr<ClientGui> gui, int driveDbId, bool displayRoot,
                                                   QWidget *parent) :
    QTreeWidget(parent), _gui(gui), _driveDbId(driveDbId), _displayRoot(displayRoot), _folderIconColor(QColor()),
    _folderIconSize(QSize()), _inserting(false) {
    initUI();
}

void BaseFolderTreeItemWidget::loadSubFolders() {
    clear();

    ExitCode exitCode;
    QList<NodeInfo> nodeInfoList;
    exitCode = GuiRequests::getSubFolders(_driveDbId, QString(), nodeInfoList);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcBaseFolderTreeItemWidget()) << "Error in Requests::getSubFolders";
        return;
    }

    if (nodeInfoList.size()) {
        CustomTreeWidgetItem *root = static_cast<CustomTreeWidgetItem *>(topLevelItem(0));
        if (!root) {
            root = new CustomTreeWidgetItem(this);

            const auto &driveInfoMapIt = _gui->driveInfoMap().find(_driveDbId);
            if (driveInfoMapIt == _gui->driveInfoMap().end()) {
                qCWarning(lcBaseFolderTreeItemWidget()) << "Sync not found in driveInfoMap for driveDbId=" << _driveDbId;
                return;
            }

            // Set drive name
            root->setText(TreeWidgetColumn::Folder, _driveDbId == 0 ? QString() : driveInfoMapIt->second.name());
            root->setIcon(TreeWidgetColumn::Folder,
                          KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/drive.svg",
                                                            driveInfoMapIt->second.color()));
            root->setData(TreeWidgetColumn::Folder, dirRole, QString());
            root->setData(TreeWidgetColumn::Folder, baseDirRole, QString());
            root->setData(TreeWidgetColumn::Folder, nodeIdRole, QString());
        }

        updateDirectories(root, nodeInfoList);

        root->setExpanded(true);
        if (!_displayRoot) {
            setRootIndex(indexFromItem(root));
        }
    } else {
        // No sub folders
        emit message(tr("No subfolders currently on the server."));
    }
}

void BaseFolderTreeItemWidget::insertNode(QTreeWidgetItem *parent, const NodeInfo &folderInfo) {
    if (folderInfo.nodeId().isEmpty()) {
        parent->setData(TreeWidgetColumn::Folder, dirRole, folderInfo.name());
        parent->setData(TreeWidgetColumn::Folder, baseDirRole, folderInfo.name());
        parent->setData(TreeWidgetColumn::Folder, nodeIdRole, QString());
    } else {
        CustomTreeWidgetItem *item = static_cast<CustomTreeWidgetItem *>(findFirstChild(parent, folderInfo.name()));
        if (!item) {
            item = new CustomTreeWidgetItem(parent);

            // Set icon
            setFolderIcon(item, folderInfo.name());

            // Set name
            item->setText(TreeWidgetColumn::Folder, folderInfo.name());

            item->setData(TreeWidgetColumn::Folder, dirRole, folderInfo.name());
            item->setData(TreeWidgetColumn::Folder, baseDirRole, folderInfo.name());
            item->setData(TreeWidgetColumn::Folder, nodeIdRole, folderInfo.nodeId());

            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }
    }
}

QList<QPair<QString, QString>> BaseFolderTreeItemWidget::buildFolderList(QTreeWidgetItem *selectedItem) {
    CustomTreeWidgetItem *root = static_cast<CustomTreeWidgetItem *>(topLevelItem(0));
    QList<QPair<QString, QString>> folderList;
    QTreeWidgetItem *item = selectedItem;
    while (item != root) {
        QString folderPath = item->data(TreeWidgetColumn::Folder, dirRole).toString();
        QString folderNodeId = item->data(TreeWidgetColumn::Folder, nodeIdRole).toString();
        QDir folderDir(folderPath);
        folderList.push_front(qMakePair(folderDir.dirName(), folderNodeId));
        item = item->parent();
    }

    return folderList;
}

void BaseFolderTreeItemWidget::initUI() {
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSortingEnabled(true);
    sortByColumn(TreeWidgetColumn::Folder, Qt::AscendingOrder);
    setItemDelegate(new CustomDelegate(this));
    setColumnCount(2);
    header()->hide();
    header()->setSectionResizeMode(TreeWidgetColumn::Folder, QHeaderView::Stretch);
    header()->setSectionResizeMode(TreeWidgetColumn::Action, QHeaderView::ResizeToContents);
    header()->setStretchLastSection(false);
    setIndentation(treeWidgetIndentation);
    setRootIsDecorated(true);

    connect(this, &QTreeWidget::itemExpanded, this, &BaseFolderTreeItemWidget::onItemExpanded);
    connect(this, &QTreeWidget::currentItemChanged, this, &BaseFolderTreeItemWidget::onCurrentItemChanged);
    connect(this, &QTreeWidget::itemClicked, this, &BaseFolderTreeItemWidget::onItemClicked);
    connect(this, &QTreeWidget::itemDoubleClicked, this, &BaseFolderTreeItemWidget::onItemDoubleClicked);
    connect(this, &QTreeWidget::itemChanged, this, &BaseFolderTreeItemWidget::onItemChanged);
}

QString BaseFolderTreeItemWidget::iconPath(const QString &folderName) {
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

QColor BaseFolderTreeItemWidget::iconColor(const QString &folderName) {
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

void BaseFolderTreeItemWidget::setFolderIcon() {
    if (_folderIconColor != QColor() && _folderIconSize != QSize()) {
        setIconSize(_folderIconSize);
        if (topLevelItem(0)) {
            setSubFoldersIcon(topLevelItem(0));
        }
    }
}

void BaseFolderTreeItemWidget::setFolderIcon(QTreeWidgetItem *item, const QString &folderName) {
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

void BaseFolderTreeItemWidget::setSubFoldersIcon(QTreeWidgetItem *parent) {
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

QTreeWidgetItem *BaseFolderTreeItemWidget::findFirstChild(QTreeWidgetItem *parent, const QString &text) {
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);
        if (child->text(TreeWidgetColumn::Folder) == text) {
            return child;
        }
    }
    return 0;
}

void BaseFolderTreeItemWidget::updateDirectories(QTreeWidgetItem *item, QList<NodeInfo> list) {
    QScopedValueRollback<bool> isInserting(_inserting);
    _inserting = true;

    // Check for excludes
    QMutableListIterator<NodeInfo> it(list);
    while (it.hasNext()) {
        bool excluded = false;
        ExitCode exitCode = GuiRequests::getNameExcluded(it.next().name(), excluded);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcBaseFolderTreeItemWidget()) << "Error in Requests::getNameExcluded";
            return;
        }
        if (excluded) {
            it.remove();
        }
    }

    KDC::CommonGuiUtility::sortSubfolders(list);
    for (NodeInfo nodeInfo: list) {
        insertNode(item, nodeInfo);
    }
}

void BaseFolderTreeItemWidget::onItemExpanded(QTreeWidgetItem *item) {
    item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicatorWhenChildless);

    QString nodeId = item->data(TreeWidgetColumn::Folder, nodeIdRole).toString();
    if (nodeId.isEmpty()) {
        return;
    }

    ExitCode exitCode;
    QList<NodeInfo> nodeInfoList;
    exitCode = GuiRequests::getSubFolders(_driveDbId, nodeId, nodeInfoList);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcBaseFolderTreeItemWidget()) << "Error in Requests::getSubFolders";
        return;
    }

    updateDirectories(item, nodeInfoList);
}

void BaseFolderTreeItemWidget::onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous) {
    if (current && !current->text(TreeWidgetColumn::Folder).isEmpty()) {
        // Add action icon
        current->setIcon(TreeWidgetColumn::Action,
                         KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/folder-add.svg", _addIconColor)
                                 .pixmap(_addIconSize));
    }

    if (previous) {
        if (previous->text(TreeWidgetColumn::Folder).isEmpty()) {
            // Remove item
            QTreeWidgetItem *parent = previous->parent();
            QTimer::singleShot(0, this, [=]() { parent->removeChild(previous); });
        } else {
            // Remove action icon
            previous->setIcon(TreeWidgetColumn::Action, QIcon());
        }
    }
}

void BaseFolderTreeItemWidget::onItemClicked(QTreeWidgetItem *item, int column) {
    if (column == TreeWidgetColumn::Action && !item->text(TreeWidgetColumn::Folder).isEmpty()) {
        // Add Folder
        CustomTreeWidgetItem *newItem = new CustomTreeWidgetItem(item);

        // Set icon
        setFolderIcon(newItem, QString());

        // Set name
        newItem->setText(TreeWidgetColumn::Folder, QString());

        // Expand item
        item->setExpanded(true);

        // Select new item
        setCurrentItem(newItem);

        // Allow editing new item name
        newItem->setFlags(newItem->flags() | Qt::ItemIsEditable);
        editItem(newItem, TreeWidgetColumn::Folder);
    } else {
        QString currentFolderBasePath = item->data(TreeWidgetColumn::Folder, baseDirRole).toString();

        QList<QPair<QString, QString>> folderList = buildFolderList(item);
        emit folderSelected(currentFolderBasePath, folderList);
    }
}

void BaseFolderTreeItemWidget::onItemDoubleClicked(QTreeWidgetItem *item, int column) {
    if (column == TreeWidgetColumn::Folder && item->flags() & Qt::ItemIsEditable) {
        // Allow editing item name
        editItem(item, TreeWidgetColumn::Folder);
    }
}

void BaseFolderTreeItemWidget::onItemChanged(QTreeWidgetItem *item, int column) {
    if (column == TreeWidgetColumn::Folder && item->flags() & Qt::ItemIsEditable) {
        // Set path
        QString parentPath = item->parent()->data(TreeWidgetColumn::Folder, dirRole).toString();
        QString path = parentPath + dirSeparator + item->text(TreeWidgetColumn::Folder);
        item->setData(TreeWidgetColumn::Folder, dirRole, path);
        QString parentBasePath = item->parent()->data(TreeWidgetColumn::Folder, baseDirRole).toString();
        item->setData(TreeWidgetColumn::Folder, baseDirRole, parentBasePath);
        onCurrentItemChanged(item, nullptr);
        scrollToItem(item);

        if (item->text(TreeWidgetColumn::Folder).isEmpty()) {
            emit noFolderSelected();
        } else {
            QList<QPair<QString, QString>> folderList = buildFolderList(item);
            emit folderSelected(parentBasePath, folderList);
        }
    }
}

BaseFolderTreeItemWidget::CustomDelegate::CustomDelegate(BaseFolderTreeItemWidget *treeWidget, QObject *parent) :
    QStyledItemDelegate(parent), _treeWidget(treeWidget) {}

void BaseFolderTreeItemWidget::CustomDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                                                    const QModelIndex &index) const {
    // Center editor into tree widget item
    QTreeWidgetItem *item = _treeWidget->itemFromIndex(index);
    QRect itemRect = _treeWidget->visualItemRect(item);

    QStyleOptionViewItem itemOption = option;
    initStyleOption(&itemOption, index);
    QRect lineEditRect = QApplication::style()->subElementRect(QStyle::SE_ItemViewItemText, &itemOption, editor);
    lineEditRect.setTop(itemRect.top() + treeWidgetEditorMargin);
    editor->setGeometry(lineEditRect);
}

} // namespace KDC
