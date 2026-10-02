/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "remotefolderpickermodel.h"

#include "app/appconstants.h"
#include "app/syncconfiguration/remotefolderprovider.h"
#include "libcommon/utility/utility.h"

#include <QCollator>
#include <QLocale>
#include <QLoggingCategory>
#include <QPointer>

#include <algorithm>
#include <chrono>
#include <utility>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcRemoteFolderPickerModel, "gui.v4.remotefolderpickermodel", QtInfoMsg)

// Quiet period after the last visibility change before visible folders list their children.
constexpr std::chrono::milliseconds childrenPrefetchDelay{150};
} // namespace

RemoteFolderPickerModel::RemoteFolderPickerModel(AbstractRemoteFolderProvider &remoteFolderProvider, QObject *const parent) :
    QAbstractItemModel(parent),
    _remoteFolderProvider(remoteFolderProvider),
    _childrenPrefetchTimer(this) {
    _childrenPrefetchTimer.setSingleShot(true);
    _childrenPrefetchTimer.setInterval(childrenPrefetchDelay);
    (void) connect(&_childrenPrefetchTimer, &QTimer::timeout, this, &RemoteFolderPickerModel::prefetchVisibleChildren);
}

QModelIndex RemoteFolderPickerModel::index(const int row, const int column, const QModelIndex &parentIndex) const {
    if (row < 0 || column != 0) {
        return {};
    }

    const TreeNode *const parentNode = nodeForIndex(parentIndex);
    if (!parentNode || static_cast<std::size_t>(row) >= parentNode->children.size()) {
        return {};
    }

    return createIndex(row, column, parentNode->children[static_cast<std::size_t>(row)].get());
}

QModelIndex RemoteFolderPickerModel::parent(const QModelIndex &child) const {
    if (!child.isValid()) {
        return {};
    }

    const auto *const node = static_cast<TreeNode *>(child.internalPointer());
    if (!node || !node->parent || node->parent == _root.get()) {
        return {};
    }

    return indexForNode(node->parent);
}

int RemoteFolderPickerModel::rowCount(const QModelIndex &parentIndex) const {
    if (parentIndex.column() > 0) {
        return 0;
    }

    const TreeNode *const node = nodeForIndex(parentIndex);
    return node ? static_cast<int>(node->children.size()) : 0;
}

int RemoteFolderPickerModel::columnCount(const QModelIndex &) const {
    return 1;
}

QVariant RemoteFolderPickerModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid()) {
        return {};
    }

    const auto *const node = static_cast<TreeNode *>(index.internalPointer());
    if (!node) {
        return {};
    }

    switch (role) {
        case NameRole:
            return node->name;
        case NodeIdRole:
            return node->nodeId;
        case DriveRootRole:
            return node->driveRoot;
        case SelectableRole:
            return isSelectable(node);
        case CanCreateFolderRole:
            return canCreateFolder(node);
        case UnavailableRole:
            return _unavailableNodeIds.contains(node->nodeId);
        case AccessDeniedRole:
            return node->accessDenied;
        case SelectedRole:
            return !node->editing && !_selectedNodeId.isEmpty() && node->nodeId == _selectedNodeId;
        case ChildrenLoadingRole:
            return node->childrenState == LoadState::Loading;
        case ChildrenLoadFailedRole:
            return node->childrenState == LoadState::Failed;
        case EditingRole:
            return node->editing;
        default:
            return {};
    }
}

QHash<int, QByteArray> RemoteFolderPickerModel::roleNames() const {
    return {{NameRole, "folderName"},
            {NodeIdRole, "nodeId"},
            {DriveRootRole, "driveRoot"},
            {SelectableRole, "selectable"},
            {CanCreateFolderRole, "canCreateFolder"},
            {UnavailableRole, "unavailable"},
            {AccessDeniedRole, "accessDenied"},
            {SelectedRole, "folderSelected"},
            {ChildrenLoadingRole, "childrenLoading"},
            {ChildrenLoadFailedRole, "childrenLoadFailed"},
            {EditingRole, "nameEditing"}};
}

bool RemoteFolderPickerModel::hasChildren(const QModelIndex &parentIndex) const {
    const TreeNode *const node = nodeForIndex(parentIndex);
    if (!node || node->accessDenied || node->editing) {
        return false;
    }

    return node->childrenState != LoadState::Loaded || !node->children.empty();
}

bool RemoteFolderPickerModel::canFetchMore(const QModelIndex &parentIndex) const {
    const TreeNode *const node = nodeForIndex(parentIndex);
    if (!node || node == _root.get() || node->accessDenied || node->editing) {
        return false;
    }

    return node->childrenState == LoadState::NotLoaded || node->childrenState == LoadState::Failed;
}

void RemoteFolderPickerModel::fetchMore(const QModelIndex &parentIndex) {
    requestChildren(nodeForIndex(parentIndex));
}

bool RemoteFolderPickerModel::loading() const {
    const TreeNode *const driveRoot = driveRootNode();
    return driveRoot && driveRoot->childrenState == LoadState::Loading;
}

bool RemoteFolderPickerModel::loadFailed() const {
    const TreeNode *const driveRoot = driveRootNode();
    return driveRoot && driveRoot->childrenState == LoadState::Failed;
}

QString RemoteFolderPickerModel::nodeIdAt(const QModelIndex &modelIndex) const {
    if (!modelIndex.isValid()) {
        return {};
    }

    return nodeForIndex(modelIndex)->nodeId;
}

QString RemoteFolderPickerModel::folderCreationParentNodeId() const {
    if (!_editingNode || !_editingNode->parent) {
        return {};
    }

    return _editingNode->parent->nodeId;
}

QString RemoteFolderPickerModel::folderCreationParentPath() const {
    if (!_editingNode || !_editingNode->parent) {
        return {};
    }

    return _editingNode->parent->path;
}

void RemoteFolderPickerModel::configure(const UserDbId userDbId, const DriveId driveId, const QString &driveName,
                                        const std::unordered_set<NodeId> &unavailableNodeIds) {
    beginResetModel();
    clearTree();
    _userDbId = userDbId;
    _driveId = driveId;
    for (const auto &nodeId: unavailableNodeIds) {
        (void) _unavailableNodeIds.insert(QString::fromStdString(nodeId));
    }

    auto driveRoot = std::make_unique<TreeNode>();
    driveRoot->nodeId = QString::fromLatin1(AppConstants::SyncConfiguration::driveRootNodeId);
    driveRoot->name = driveName;
    driveRoot->parent = _root.get();
    driveRoot->driveRoot = true;
    (void) _nodesById.insert(driveRoot->nodeId, driveRoot.get());
    _root->children.push_back(std::move(driveRoot));
    endResetModel();

    emit selectionChanged();
    emit folderCreationChanged();
    requestChildren(driveRootNode());
}

void RemoteFolderPickerModel::reset() {
    beginResetModel();
    clearTree();
    endResetModel();

    emit stateChanged();
    emit selectionChanged();
    emit folderCreationChanged();
}

void RemoteFolderPickerModel::restoreSelection(const NodeId &nodeId, const QString &name, const QString &path) {
    setSelection(QString::fromStdString(nodeId), name, path);
}

void RemoteFolderPickerModel::setFolderCreationPending(const bool pending) {
    if (!_editingNode || _editingNode->creating == pending) {
        return;
    }

    _editingNode->creating = pending;
    emit folderCreationChanged();
}

void RemoteFolderPickerModel::insertCreatedFolder(const NodeInfo &info) {
    if (!_editingNode || info.nodeId().isEmpty()) {
        qCWarning(lcRemoteFolderPickerModel) << "Created folder ignored: no editing row or no node id | nodeId:" << info.nodeId();
        return;
    }

    TreeNode *const parentNode = _editingNode->parent;
    removeEditingRow();

    // A listing of the parent that completed meanwhile can already contain the new folder.
    if (!_nodesById.contains(info.nodeId())) {
        auto child = std::make_unique<TreeNode>();
        child->nodeId = info.nodeId();
        child->name = info.name();
        child->path = info.path();
        child->parent = parentNode;
        // A folder that was just created has no sub-folder yet.
        child->childrenState = LoadState::Loaded;

        const std::size_t row = sortedInsertionRow(parentNode, child->name);
        beginInsertRows(indexForNode(parentNode), static_cast<int>(row), static_cast<int>(row));
        (void) _nodesById.insert(child->nodeId, child.get());
        (void) parentNode->children.insert(parentNode->children.begin() + static_cast<std::ptrdiff_t>(row), std::move(child));
        endInsertRows();
    }

    const TreeNode *const createdNode = _nodesById.value(info.nodeId(), nullptr);
    setSelection(createdNode->nodeId, createdNode->name, createdNode->path);
    emit folderCreationChanged();
    emit folderCreated(indexForNode(createdNode));
}

void RemoteFolderPickerModel::retryRoot() {
    TreeNode *const driveRoot = driveRootNode();
    if (driveRoot && driveRoot->childrenState == LoadState::Failed) {
        requestChildren(driveRoot);
    }
}

void RemoteFolderPickerModel::retryChildren(const QModelIndex &modelIndex) {
    TreeNode *const node = nodeForIndex(modelIndex);
    if (node && node != _root.get() && node->childrenState == LoadState::Failed) {
        requestChildren(node);
    }
}

bool RemoteFolderPickerModel::select(const QModelIndex &modelIndex) {
    const TreeNode *const node = nodeForIndex(modelIndex);
    if (!node || node == _root.get()) {
        return false;
    }

    if (!isSelectable(node)) {
        setSelection({}, {}, {});
        return false;
    }

    setSelection(node->nodeId, node->name, node->path);
    return true;
}

void RemoteFolderPickerModel::setNodeVisible(const QString &nodeId, const bool visible) {
    if (!visible) {
        (void) _childrenPrefetchCandidates.remove(nodeId);
        // Leaving the viewport is a visibility change too: the remaining candidates wait for the next quiet period.
        if (_childrenPrefetchCandidates.isEmpty()) {
            _childrenPrefetchTimer.stop();
        } else {
            _childrenPrefetchTimer.start();
        }
        return;
    }

    if (const TreeNode *const node = _nodesById.value(nodeId, nullptr); !node || node->childrenState != LoadState::NotLoaded) {
        return;
    }

    (void) _childrenPrefetchCandidates.insert(nodeId);
    // Restarted on every change, so it only fires once scrolling stops.
    _childrenPrefetchTimer.start();
}

void RemoteFolderPickerModel::prefetchVisibleChildren() {
    const QSet<QString> candidates = std::exchange(_childrenPrefetchCandidates, {});
    for (const QString &nodeId: candidates) {
        requestChildren(_nodesById.value(nodeId, nullptr));
    }
}

bool RemoteFolderPickerModel::beginFolderCreation(const QModelIndex &parentIndex) {
    TreeNode *const parentNode = nodeForIndex(parentIndex);
    if (!parentNode || parentNode == _root.get() || !canCreateFolder(parentNode) || folderCreationPending()) {
        return false;
    }

    removeEditingRow();
    _pendingCreationParentNodeId.clear();

    if (parentNode->childrenState == LoadState::Loaded) {
        insertEditingRow(parentNode);
        return true;
    }

    // The editing row is inserted once the listing arrives, so it cannot be followed by folders loaded after it.
    _pendingCreationParentNodeId = parentNode->nodeId;
    requestChildren(parentNode);
    return true;
}

void RemoteFolderPickerModel::cancelFolderCreation() {
    _pendingCreationParentNodeId.clear();
    if (!_editingNode || _editingNode->creating) {
        return;
    }

    removeEditingRow();
    emit folderCreationChanged();
}

// Must run between beginResetModel() and endResetModel().
void RemoteFolderPickerModel::clearTree() {
    ++_generation;
    _childrenPrefetchTimer.stop();
    _childrenPrefetchCandidates.clear();
    _userDbId = 0;
    _driveId = 0;
    _nodesById.clear();
    _unavailableNodeIds.clear();
    _editingNode = nullptr;
    _pendingCreationParentNodeId.clear();
    _selectedNodeId.clear();
    _selectedName.clear();
    _selectedPath.clear();
    _root = std::make_unique<TreeNode>();
    _root->childrenState = LoadState::Loaded;
}

RemoteFolderPickerModel::TreeNode *RemoteFolderPickerModel::nodeForIndex(const QModelIndex &modelIndex) const {
    return modelIndex.isValid() ? static_cast<TreeNode *>(modelIndex.internalPointer()) : _root.get();
}

QModelIndex RemoteFolderPickerModel::indexForNode(const TreeNode *const node) const {
    if (!node || !node->parent || node == _root.get()) {
        return {};
    }

    const auto &siblings = node->parent->children;
    const auto it = std::ranges::find_if(siblings, [node](const auto &candidate) { return candidate.get() == node; });
    if (it == siblings.end()) {
        return {};
    }

    return createIndex(static_cast<int>(std::distance(siblings.begin(), it)), 0, node);
}

RemoteFolderPickerModel::TreeNode *RemoteFolderPickerModel::driveRootNode() const {
    return _root->children.empty() ? nullptr : _root->children.front().get();
}

/**
 * The drive root is excluded because an advanced synchronization targets a sub-folder: the server would validate a drive
 * root target as a classic synchronization.
 */
bool RemoteFolderPickerModel::isSelectable(const TreeNode *const node) const {
    return !node->driveRoot && !node->editing && !node->accessDenied && !_unavailableNodeIds.contains(node->nodeId);
}

/**
 * The drive root can host a new folder although it cannot be selected. A folder already targeted by a synchronization
 * cannot: the new folder would lead to a synchronization nested in another one's remote folder. It would not be
 * synchronized twice, though, since the server blacklists a created folder in every synchronization of the drive.
 */
bool RemoteFolderPickerModel::canCreateFolder(const TreeNode *const node) const {
    return !node->editing && !node->accessDenied && !_unavailableNodeIds.contains(node->nodeId);
}

void RemoteFolderPickerModel::requestChildren(TreeNode *const node) {
    if (!node || node == _root.get() || node->accessDenied || node->editing) {
        return;
    }

    if (node->childrenState != LoadState::NotLoaded && node->childrenState != LoadState::Failed) {
        return;
    }

    node->childrenState = LoadState::Loading;
    notifyChildrenState(node);

    // The drive root is listed through the root listing request, which the daemon selects for an empty node id.
    const NodeId requestedNodeId = node->driveRoot ? NodeId{} : QStr2Str(node->nodeId);
    const uint64_t generation = _generation;
    const QPointer self(this);
    _remoteFolderProvider.requestChildren(
            _userDbId, _driveId, requestedNodeId,
            [self, node, generation](const ExitInfo &exitInfo, const std::vector<NodeInfo> &children) {
                if (!self || generation != self->_generation) {
                    return;
                }

                self->handleChildrenResult(node, generation, static_cast<bool>(exitInfo), children);
            });
}

void RemoteFolderPickerModel::handleChildrenResult(TreeNode *const node, const uint64_t generation, const bool success,
                                                   const std::vector<NodeInfo> &children) {
    if (generation != _generation || !node || node->childrenState != LoadState::Loading) {
        return;
    }

    if (!success) {
        node->childrenState = LoadState::Failed;
        notifyChildrenState(node);
        if (_pendingCreationParentNodeId == node->nodeId) {
            _pendingCreationParentNodeId.clear();
            emit folderCreationChanged();
        }
        return;
    }

    std::vector<NodeInfo> sortedChildren = children;
    // Folder names are user data rather than translated UI text: keep their ordering tied to the system locale, as the
    // selective synchronization tree does.
    const QCollator systemCollator(QLocale::system());
    (void) std::ranges::sort(sortedChildren, [&systemCollator](const NodeInfo &lhs, const NodeInfo &rhs) {
        return systemCollator.compare(lhs.name(), rhs.name()) < 0;
    });

    const QModelIndex parentIndex = indexForNode(node);
    if (!sortedChildren.empty()) {
        const auto firstRow = static_cast<int>(node->children.size());
        beginInsertRows(parentIndex, firstRow, firstRow + static_cast<int>(sortedChildren.size()) - 1);
        for (const auto &info: sortedChildren) {
            auto child = std::make_unique<TreeNode>();
            child->nodeId = info.nodeId();
            child->name = info.name();
            child->path = info.path();
            child->parent = node;
            child->accessDenied = info.accessDenied();
            (void) _nodesById.insert(child->nodeId, child.get());
            node->children.push_back(std::move(child));
        }
        endInsertRows();
    }

    node->childrenState = LoadState::Loaded;
    notifyChildrenState(node);

    // The restored selection may belong to this branch.
    if (!_selectedNodeId.isEmpty()) {
        notifySelectedRow(_selectedNodeId);
    }

    if (_pendingCreationParentNodeId == node->nodeId) {
        _pendingCreationParentNodeId.clear();
        insertEditingRow(node);
    }
}

void RemoteFolderPickerModel::notifyChildrenState(const TreeNode *const node) {
    if (node->driveRoot) {
        emit stateChanged();
    }

    const QModelIndex nodeIndex = indexForNode(node);
    emit dataChanged(nodeIndex, nodeIndex, {ChildrenLoadingRole, ChildrenLoadFailedRole});
}

void RemoteFolderPickerModel::insertEditingRow(TreeNode *const parentNode) {
    auto editingNode = std::make_unique<TreeNode>();
    editingNode->parent = parentNode;
    editingNode->editing = true;
    editingNode->childrenState = LoadState::Loaded;

    const QModelIndex parentIndex = indexForNode(parentNode);
    beginInsertRows(parentIndex, 0, 0);
    _editingNode = editingNode.get();
    (void) parentNode->children.insert(parentNode->children.begin(), std::move(editingNode));
    endInsertRows();

    emit folderCreationChanged();
    emit folderCreationStarted(parentIndex);
}

void RemoteFolderPickerModel::removeEditingRow() {
    if (!_editingNode) {
        return;
    }

    TreeNode *const parentNode = _editingNode->parent;
    const QModelIndex editingIndex = indexForNode(_editingNode);
    beginRemoveRows(indexForNode(parentNode), editingIndex.row(), editingIndex.row());
    _editingNode = nullptr;
    (void) parentNode->children.erase(parentNode->children.begin() + editingIndex.row());
    endRemoveRows();
}

std::size_t RemoteFolderPickerModel::sortedInsertionRow(const TreeNode *const parentNode, const QString &name) const {
    const QCollator systemCollator(QLocale::system());
    const auto &siblings = parentNode->children;
    const auto it = std::ranges::find_if(siblings, [&systemCollator, &name](const auto &sibling) {
        return !sibling->editing && systemCollator.compare(name, sibling->name) < 0;
    });

    return static_cast<std::size_t>(std::distance(siblings.begin(), it));
}

void RemoteFolderPickerModel::setSelection(const QString &nodeId, const QString &name, const QString &path) {
    if (nodeId == _selectedNodeId && name == _selectedName && path == _selectedPath) {
        return;
    }

    const QString previousNodeId = std::exchange(_selectedNodeId, nodeId);
    _selectedName = name;
    _selectedPath = path;

    notifySelectedRow(previousNodeId);
    notifySelectedRow(_selectedNodeId);
    emit selectionChanged();
}

void RemoteFolderPickerModel::notifySelectedRow(const QString &nodeId) {
    const TreeNode *const node = _nodesById.value(nodeId, nullptr);
    if (!node) {
        return;
    }

    const QModelIndex nodeIndex = indexForNode(node);
    emit dataChanged(nodeIndex, nodeIndex, {SelectedRole});
}

} // namespace KDC
