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

#include "remotefoldertreemodel.h"

#include "app/syncconfiguration/remotefolderprovider.h"
#include "libcommon/info/nodeinfo.h"
#include "libcommon/utility/utility.h"

#include <QLocale>
#include <QPointer>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace KDC {

namespace {
constexpr uint8_t maxConcurrentSizeRequests = 4;
constexpr QStringView unavailableSize = u"—";
} // namespace

RemoteFolderTreeModel::RemoteFolderTreeModel(AbstractRemoteFolderProvider &remoteFolderProvider, QObject *const parent) :
    QAbstractItemModel(parent),
    _remoteFolderProvider(remoteFolderProvider) {}

QModelIndex RemoteFolderTreeModel::index(const int row, const int column, const QModelIndex &parentIndex) const {
    if (row < 0 || column != 0) return {};
    const TreeNode *const parentNode = nodeForIndex(parentIndex);
    if (!parentNode || static_cast<std::size_t>(row) >= parentNode->children.size()) return {};
    return createIndex(row, column, parentNode->children[static_cast<std::size_t>(row)].get());
}

QModelIndex RemoteFolderTreeModel::parent(const QModelIndex &child) const {
    if (!child.isValid()) return {};
    const auto *const node = static_cast<TreeNode *>(child.internalPointer());
    if (!node || !node->parent || node->parent == _root.get()) return {};
    return indexForNode(node->parent);
}

int RemoteFolderTreeModel::rowCount(const QModelIndex &parentIndex) const {
    if (parentIndex.column() > 0) return 0;
    const TreeNode *const node = nodeForIndex(parentIndex);
    return node ? static_cast<int>(node->children.size()) : 0;
}

int RemoteFolderTreeModel::columnCount(const QModelIndex &) const {
    return 1;
}

QVariant RemoteFolderTreeModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid()) return {};
    const auto *const node = static_cast<TreeNode *>(index.internalPointer());
    if (!node) return {};

    switch (role) {
        case NameRole:
            return node->name;
        case NodeIdRole:
            return node->nodeId;
        case CheckStateRole:
            return static_cast<int>(checkState(node));
        case AccessDeniedRole:
            return node->accessDenied;
        case SizeTextRole:
            // A dash until the size is known, whether it is still loading or failed to load: the column then keeps a
            // steady shape while sizes arrive, instead of filling up cell by cell from an empty state.
            return node->sizeState == SizeState::Loaded ? QLocale{}.formattedDataSize(node->size, 1, QLocale::DataSizeSIFormat)
                                                        : unavailableSize.toString();
        case ChildrenLoadingRole:
            return node->childrenState == LoadState::Loading;
        case ChildrenLoadFailedRole:
            return node->childrenState == LoadState::Failed;
        default:
            return {};
    }
}

QHash<int, QByteArray> RemoteFolderTreeModel::roleNames() const {
    return {{NameRole, "folderName"},
            {NodeIdRole, "nodeId"},
            {CheckStateRole, "checkState"},
            {AccessDeniedRole, "accessDenied"},
            {SizeTextRole, "sizeText"},
            {ChildrenLoadingRole, "childrenLoading"},
            {ChildrenLoadFailedRole, "childrenLoadFailed"}};
}

bool RemoteFolderTreeModel::hasChildren(const QModelIndex &parentIndex) const {
    const TreeNode *const node = nodeForIndex(parentIndex);
    return node && !node->accessDenied && (node->childrenState != LoadState::Loaded || !node->children.empty());
}

bool RemoteFolderTreeModel::canFetchMore(const QModelIndex &parentIndex) const {
    const TreeNode *const node = nodeForIndex(parentIndex);
    return node && !node->accessDenied && (node != _root.get() || _initialPathsState == InitialPathsState::Ready) &&
           (node->childrenState == LoadState::NotLoaded || node->childrenState == LoadState::Failed);
}

void RemoteFolderTreeModel::fetchMore(const QModelIndex &parentIndex) {
    requestChildren(nodeForIndex(parentIndex));
}

bool RemoteFolderTreeModel::loading() const {
    return _initialPathsState == InitialPathsState::Resolving || _root->childrenState == LoadState::Loading;
}

bool RemoteFolderTreeModel::loadFailed() const {
    return _initialPathsState == InitialPathsState::Failed || _root->childrenState == LoadState::Failed;
}

bool RemoteFolderTreeModel::empty() const {
    return _root->childrenState == LoadState::Loaded && _root->children.empty();
}

int RemoteFolderTreeModel::rootCheckState() const {
    return static_cast<int>(_excludedNodeIds.isEmpty() ? Qt::Checked : Qt::PartiallyChecked);
}

void RemoteFolderTreeModel::configure(const UserDbId userDbId, const DriveId driveId, const NodeId &rootNodeId,
                                      const std::vector<NodeId> &initialBlackList) {
    beginResetModel();
    ++_generation;
    _userDbId = userDbId;
    _driveId = driveId;
    _root = std::make_unique<TreeNode>();
    _root->nodeId = QString::fromStdString(rootNodeId);
    _nodesById.clear();
    _excludedNodeIds.clear();
    _excludedPaths.clear();
    _sizeQueue.clear();
    // `_activeSizeRequests` is deliberately kept: the requests of the previous generation are still in flight and
    // still occupy the provider, so resetting it here would let this generation start as many again.
    _pendingInitialPathRequests = 0;
    for (const auto &nodeId: initialBlackList) (void) _excludedNodeIds.insert(QString::fromStdString(nodeId));
    _initialPathsState = _excludedNodeIds.isEmpty() ? InitialPathsState::Ready : InitialPathsState::Resolving;
    _initialPathRequestFailed = false;
    endResetModel();
    emit selectionChanged();
    resolveInitialExclusionPaths();
}

std::vector<NodeId> RemoteFolderTreeModel::blackList() const {
    QStringList sortedIds(_excludedNodeIds.cbegin(), _excludedNodeIds.cend());
    sortedIds.sort(Qt::CaseSensitive);
    std::vector<NodeId> result;
    result.reserve(static_cast<std::size_t>(sortedIds.size()));
    for (const auto &nodeId: sortedIds) result.push_back(QStr2Str(nodeId));
    return result;
}

void RemoteFolderTreeModel::retryRoot() {
    if (_initialPathsState == InitialPathsState::Failed) {
        _initialPathsState = InitialPathsState::Resolving;
        _initialPathRequestFailed = false;
        emit stateChanged();
        resolveInitialExclusionPaths();
        return;
    }
    if (_root->childrenState == LoadState::Failed) requestChildren(_root.get());
}

void RemoteFolderTreeModel::retryChildren(const QModelIndex &modelIndex) {
    if (TreeNode *const node = nodeForIndex(modelIndex); node && node->childrenState == LoadState::Failed) requestChildren(node);
}

void RemoteFolderTreeModel::toggleSelection(const QModelIndex &modelIndex) {
    TreeNode *const node = nodeForIndex(modelIndex);
    if (!node || node == _root.get() || node->accessDenied) return;
    if (checkState(node) == Qt::Unchecked) {
        includeNode(node);
    } else {
        excludeNode(node);
    }
    notifySelectionDataChanged();
    emit selectionChanged();
}

void RemoteFolderTreeModel::toggleRootSelection() {
    if (_root->children.empty()) return;
    if (_excludedNodeIds.isEmpty()) {
        for (const auto &child: _root->children) excludeNode(child.get());
    } else {
        _excludedNodeIds.clear();
        _excludedPaths.clear();
    }
    notifySelectionDataChanged();
    emit selectionChanged();
}

void RemoteFolderTreeModel::setRowVisible(const QModelIndex &modelIndex, const bool visible) {
    TreeNode *const node = nodeForIndex(modelIndex);
    if (!node || node == _root.get()) return;
    node->sizeRequested = visible;
    if (!visible) {
        if (node->sizeState == SizeState::Queued) node->sizeState = SizeState::NotRequested;
        return;
    }
    queueSize(node);
    if (node->childrenState == LoadState::NotLoaded) requestChildren(node);
}

RemoteFolderTreeModel::TreeNode *RemoteFolderTreeModel::nodeForIndex(const QModelIndex &modelIndex) const {
    return modelIndex.isValid() ? static_cast<TreeNode *>(modelIndex.internalPointer()) : _root.get();
}

QModelIndex RemoteFolderTreeModel::indexForNode(const TreeNode *const node) const {
    if (!node || !node->parent || node == _root.get()) return {};
    const auto &siblings = node->parent->children;
    const auto it = std::ranges::find_if(siblings, [node](const auto &candidate) { return candidate.get() == node; });
    if (it == siblings.end()) return {};
    return createIndex(static_cast<int>(std::distance(siblings.begin(), it)), 0, node);
}

Qt::CheckState RemoteFolderTreeModel::checkState(const TreeNode *const node) const {
    if (isExcluded(node)) return Qt::Unchecked;
    return hasExcludedDescendant(node) ? Qt::PartiallyChecked : Qt::Checked;
}

bool RemoteFolderTreeModel::isExcluded(const TreeNode *const node) const {
    for (const TreeNode *cursor = node; cursor && cursor != _root.get(); cursor = cursor->parent) {
        if (_excludedNodeIds.contains(cursor->nodeId)) return true;
    }
    return false;
}

bool RemoteFolderTreeModel::hasExcludedDescendant(const TreeNode *const node) const {
    for (auto it = _excludedPaths.cbegin(); it != _excludedPaths.cend(); ++it) {
        if (pathContains(node->path, it.value()) && node->path != it.value()) return true;
    }
    return false;
}

bool RemoteFolderTreeModel::pathContains(const QString &ancestorPath, const QString &descendantPath) {
    if (ancestorPath.isEmpty()) return !descendantPath.isEmpty();
    if (ancestorPath == descendantPath) return true;
    QString prefix = ancestorPath;
    if (!prefix.endsWith(u'/')) prefix += u'/';
    return descendantPath.startsWith(prefix);
}

QString RemoteFolderTreeModel::effectivePath(const NodeInfo &info, const TreeNode *const parentNode) const {
    if (!info.path().isEmpty()) return info.path();
    if (!parentNode || parentNode == _root.get() || parentNode->path.isEmpty()) return u"/"_s + info.name();
    return parentNode->path + u'/' + info.name();
}

// An unresolved path would make every ancestor of the excluded folder look completely selected, so a failure here
// fails the whole page instead of displaying a selection that does not match what will be synchronized.
void RemoteFolderTreeModel::resolveInitialExclusionPaths() {
    if (_excludedNodeIds.isEmpty()) {
        requestChildren(_root.get());
        return;
    }

    _pendingInitialPathRequests = static_cast<uint32_t>(_excludedNodeIds.size());
    emit stateChanged();
    const uint64_t generation = _generation;
    const QPointer self(this);
    for (const auto &nodeId: QStringList(_excludedNodeIds.cbegin(), _excludedNodeIds.cend())) {
        _remoteFolderProvider.requestNodeInfo(_userDbId, _driveId, QStr2Str(nodeId),
                                              [self, generation, nodeId](const ExitInfo &exitInfo, const NodeInfo &info) {
                                                  if (!self || generation != self->_generation) return;
                                                  self->handleInitialExclusionPathResult(nodeId, exitInfo, info);
                                              });
    }
}

void RemoteFolderTreeModel::handleInitialExclusionPathResult(const QString &nodeId, const ExitInfo &exitInfo,
                                                             const NodeInfo &info) {
    if (_pendingInitialPathRequests > 0) --_pendingInitialPathRequests;

    if (exitInfo && !info.path().isEmpty()) {
        (void) _excludedPaths.insert(nodeId, info.path());
    } else if (!exitInfo && exitInfo.cause() == ExitCause::NotFound) {
        // The folder was deleted remotely since it was blacklisted: dropping it keeps the blacklist canonical.
        (void) _excludedNodeIds.remove(nodeId);
        (void) _excludedPaths.remove(nodeId);
    } else {
        _initialPathRequestFailed = true;
    }

    if (_pendingInitialPathRequests > 0) return;
    if (_initialPathRequestFailed) {
        _initialPathsState = InitialPathsState::Failed;
        _excludedPaths.clear();
        emit stateChanged();
        return;
    }
    _initialPathsState = InitialPathsState::Ready;
    emit selectionChanged();
    requestChildren(_root.get());
}

void RemoteFolderTreeModel::requestChildren(TreeNode *const node) {
    if (!node || node->accessDenied) return;
    if (node->childrenState != LoadState::NotLoaded && node->childrenState != LoadState::Failed) return;
    if (node == _root.get() && _initialPathsState != InitialPathsState::Ready) return;
    node->childrenState = LoadState::Loading;
    if (node == _root.get())
        emit stateChanged();
    else
        emit dataChanged(indexForNode(node), indexForNode(node), {ChildrenLoadingRole, ChildrenLoadFailedRole});

    const uint64_t generation = _generation;
    const QPointer self(this);
    _remoteFolderProvider.requestChildren(_userDbId, _driveId, QStr2Str(node->nodeId),
                                          [self, node, generation](const bool success, const std::vector<NodeInfo> &children) {
                                              if (!self || generation != self->_generation) return;
                                              self->handleChildrenResult(node, generation, success, children);
                                          });
}

void RemoteFolderTreeModel::handleChildrenResult(TreeNode *const node, const uint64_t generation, const bool success,
                                                 const std::vector<NodeInfo> &children) {
    if (generation != _generation || !node) return;
    if (!success) {
        node->childrenState = LoadState::Failed;
        if (node == _root.get())
            emit stateChanged();
        else
            emit dataChanged(indexForNode(node), indexForNode(node), {ChildrenLoadingRole, ChildrenLoadFailedRole});
        return;
    }

    std::vector<NodeInfo> sortedChildren = children;
    (void) std::ranges::sort(sortedChildren, [](const NodeInfo &lhs, const NodeInfo &rhs) {
        return QString::localeAwareCompare(lhs.name(), rhs.name()) < 0;
    });
    const QModelIndex parentIndex = indexForNode(node);
    if (!sortedChildren.empty()) beginInsertRows(parentIndex, 0, static_cast<int>(sortedChildren.size()) - 1);
    for (const auto &info: sortedChildren) {
        auto child = std::make_unique<TreeNode>();
        child->nodeId = info.nodeId();
        child->name = info.name();
        child->path = effectivePath(info, node);
        child->parent = node;
        child->accessDenied = info.accessDenied();
        if (_excludedNodeIds.contains(child->nodeId)) (void) _excludedPaths.insert(child->nodeId, child->path);
        (void) _nodesById.insert(child->nodeId, child.get());
        node->children.push_back(std::move(child));
    }
    if (!sortedChildren.empty()) endInsertRows();
    node->childrenState = LoadState::Loaded;
    if (node == _root.get())
        emit stateChanged();
    else
        emit dataChanged(indexForNode(node), indexForNode(node), {ChildrenLoadingRole, ChildrenLoadFailedRole});

    notifySelectionDataChanged();
}

void RemoteFolderTreeModel::excludeNode(const TreeNode *const node) {
    removeExclusionsAtOrBelow(node);
    (void) _excludedNodeIds.insert(node->nodeId);
    (void) _excludedPaths.insert(node->nodeId, node->path);
}

void RemoteFolderTreeModel::includeNode(TreeNode *const node) {
    const TreeNode *excludedAncestor = nullptr;
    for (TreeNode *cursor = node; cursor && cursor != _root.get(); cursor = cursor->parent) {
        if (_excludedNodeIds.contains(cursor->nodeId)) {
            excludedAncestor = cursor;
            break;
        }
    }
    if (excludedAncestor && excludedAncestor != node) includeNodeUnderExcludedAncestor(node, excludedAncestor);
    removeExclusionsAtOrBelow(node);
}

void RemoteFolderTreeModel::includeNodeUnderExcludedAncestor(const TreeNode *const node, const TreeNode *const excludedAncestor) {
    (void) _excludedNodeIds.remove(excludedAncestor->nodeId);
    (void) _excludedPaths.remove(excludedAncestor->nodeId);
    const TreeNode *cursor = node;
    while (cursor && cursor != excludedAncestor) {
        const TreeNode *const parentNode = cursor->parent;
        if (!parentNode) break;
        for (const auto &sibling: parentNode->children) {
            if (sibling.get() != cursor) excludeNode(sibling.get());
        }
        cursor = parentNode;
    }
}

void RemoteFolderTreeModel::removeExclusionsAtOrBelow(const TreeNode *const node) {
    QList<QString> idsToRemove;
    for (auto it = _excludedNodeIds.cbegin(); it != _excludedNodeIds.cend(); ++it) {
        if (const QString path = _excludedPaths.value(*it);
            *it == node->nodeId || (!path.isEmpty() && pathContains(node->path, path)))
            idsToRemove.push_back(*it);
    }
    for (const auto &id: idsToRemove) {
        (void) _excludedNodeIds.remove(id);
        (void) _excludedPaths.remove(id);
    }
}

void RemoteFolderTreeModel::notifySelectionDataChanged() {
    notifySelectionDataChanged(_root.get());
}

void RemoteFolderTreeModel::notifySelectionDataChanged(const TreeNode *const parentNode) {
    if (!parentNode || parentNode->children.empty()) return;
    emit dataChanged(indexForNode(parentNode->children.front().get()), indexForNode(parentNode->children.back().get()),
                     {CheckStateRole});
    for (const auto &child: parentNode->children) notifySelectionDataChanged(child.get());
}

void RemoteFolderTreeModel::queueSize(TreeNode *const node) {
    if (!node || node->sizeState != SizeState::NotRequested) return;
    node->sizeState = SizeState::Queued;
    _sizeQueue.enqueue(node->nodeId);
    processSizeQueue();
}

void RemoteFolderTreeModel::processSizeQueue() {
    while (_activeSizeRequests < maxConcurrentSizeRequests && !_sizeQueue.isEmpty()) {
        const QString nodeId = _sizeQueue.dequeue();
        TreeNode *const node = _nodesById.value(nodeId, nullptr);
        if (!node || node->sizeState != SizeState::Queued || !node->sizeRequested) continue;
        node->sizeState = SizeState::Loading;
        ++_activeSizeRequests;
        const uint64_t generation = _generation;
        const QPointer self(this);
        _remoteFolderProvider.requestSize(_userDbId, _driveId, QStr2Str(nodeId),
                                          [self, nodeId, generation](const bool success, const int64_t size) {
                                              if (!self) return;
                                              self->handleSizeResult(nodeId, generation, success, size);
                                          });
    }
}

void RemoteFolderTreeModel::handleSizeResult(const QString &nodeId, const uint64_t generation, const bool success,
                                             const qint64 size) {
    // The counter tracks every request in flight, whatever its generation: a result of a previous configuration
    // frees a slot on the provider just the same, and only its payload is dropped.
    if (_activeSizeRequests > 0) --_activeSizeRequests;
    if (generation == _generation) {
        if (TreeNode *const node = _nodesById.value(nodeId, nullptr)) {
            node->sizeState = success ? SizeState::Loaded : SizeState::Failed;
            node->size = size;
            emit dataChanged(indexForNode(node), indexForNode(node), {SizeTextRole});
        }
    }
    processSizeQueue();
}

} // namespace KDC
