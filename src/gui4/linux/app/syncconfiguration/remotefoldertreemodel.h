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

#pragma once

#include "libcommon/utility/types.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QQueue>
#include <QSet>

#include <memory>
#include <vector>

namespace KDC {

class AbstractRemoteFolderProvider;
class NodeInfo;

/**
 * Reusable remote-folder tree for selective synchronization.
 *
 * The model owns no onboarding state. A caller configures it with remote identifiers and an initial canonical
 * blacklist, then reads the resulting blacklist after the user validates the editor.
 */
class RemoteFolderTreeModel final : public QAbstractItemModel {
        Q_OBJECT
        Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
        Q_PROPERTY(bool loadFailed READ loadFailed NOTIFY stateChanged)
        Q_PROPERTY(bool empty READ empty NOTIFY stateChanged)
        Q_PROPERTY(int rootCheckState READ rootCheckState NOTIFY selectionChanged)

    public:
        enum Role : int32_t {
            NameRole = Qt::UserRole + 1,
            NodeIdRole,
            CheckStateRole,
            AccessDeniedRole,
            SizeTextRole,
            ChildrenLoadingRole,
            ChildrenLoadFailedRole,
        };
        Q_ENUM(Role)

        explicit RemoteFolderTreeModel(AbstractRemoteFolderProvider &remoteFolderProvider, QObject *parent = nullptr);

        [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex &parentIndex = QModelIndex()) const override;
        [[nodiscard]] QModelIndex parent(const QModelIndex &child) const override;
        [[nodiscard]] int rowCount(const QModelIndex &parentIndex = QModelIndex()) const override;
        [[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
        [[nodiscard]] bool hasChildren(const QModelIndex &parentIndex = QModelIndex()) const override;
        [[nodiscard]] bool canFetchMore(const QModelIndex &parentIndex) const override;
        void fetchMore(const QModelIndex &parentIndex) override;

        [[nodiscard]] bool loading() const;
        [[nodiscard]] bool loadFailed() const;
        [[nodiscard]] bool empty() const;
        [[nodiscard]] int rootCheckState() const;

        void configure(UserDbId userDbId, DriveId driveId, const NodeId &rootNodeId, const std::vector<NodeId> &initialBlackList);
        [[nodiscard]] std::vector<NodeId> blackList() const;

        /** Parent of a row, for keyboard navigation: QAbstractItemModel::parent() is not callable from QML. */
        Q_INVOKABLE [[nodiscard]] QModelIndex parentIndex(const QModelIndex &modelIndex) const { return parent(modelIndex); }

        Q_INVOKABLE void retryRoot();
        Q_INVOKABLE void retryChildren(const QModelIndex &modelIndex);
        /**
         * Includes a folder with its complete subtree, or excludes it with its complete subtree.
         *
         * A partially checked folder is excluded as a whole, like an included one: the partial state reports that a
         * descendant is excluded, it is never a state the user selects.
         */
        Q_INVOKABLE void toggleSelection(const QModelIndex &modelIndex);
        /** Selects or deselects every folder directly below the drive root. The drive root itself can never be excluded. */
        Q_INVOKABLE void toggleRootSelection();
        /**
         * Reports that a row entered or left the viewport.
         *
         * A visible row loads its folder size and its immediate children, so its expand affordance reflects whether
         * the folder really has sub-folders instead of assuming it does.
         */
        Q_INVOKABLE void setRowVisible(const QModelIndex &modelIndex, bool visible);

    signals:
        void stateChanged();
        void selectionChanged();

    private:
        enum class LoadState : uint8_t {
            NotLoaded,
            Loading,
            Loaded,
            Failed,
        };

        enum class InitialPathsState : uint8_t {
            Ready,
            Resolving,
            Failed,
        };

        enum class SizeState : uint8_t {
            NotRequested,
            Queued,
            Loading,
            Loaded,
            Failed,
        };

        struct TreeNode {
                QString nodeId;
                QString name;
                QString path;
                TreeNode *parent{nullptr};
                std::vector<std::unique_ptr<TreeNode>> children;
                LoadState childrenState{LoadState::NotLoaded};
                SizeState sizeState{SizeState::NotRequested};
                qint64 size{0};
                bool accessDenied{false};
                bool sizeRequested{false};
        };

        [[nodiscard]] TreeNode *nodeForIndex(const QModelIndex &modelIndex) const;
        [[nodiscard]] QModelIndex indexForNode(const TreeNode *node) const;
        [[nodiscard]] Qt::CheckState checkState(const TreeNode *node) const;
        [[nodiscard]] bool isExcluded(const TreeNode *node) const;
        [[nodiscard]] bool hasExcludedDescendant(const TreeNode *node) const;
        [[nodiscard]] static bool pathContains(const QString &ancestorPath, const QString &descendantPath);
        [[nodiscard]] QString effectivePath(const NodeInfo &info, const TreeNode *parentNode) const;
        void resolveInitialExclusionPaths();
        void handleInitialExclusionPathResult(const QString &nodeId, const ExitInfo &exitInfo, const NodeInfo &info);
        void requestChildren(TreeNode *node);
        void handleChildrenResult(TreeNode *node, uint64_t generation, bool success, const std::vector<NodeInfo> &children);
        void excludeNode(const TreeNode *node);
        void includeNode(TreeNode *node);
        void includeNodeUnderExcludedAncestor(const TreeNode *node, const TreeNode *excludedAncestor);
        void removeExclusionsAtOrBelow(const TreeNode *node);
        void notifySelectionDataChanged();
        void notifySelectionDataChanged(const TreeNode *parentNode);
        void queueSize(TreeNode *node);
        void processSizeQueue();
        void handleSizeResult(const QString &nodeId, uint64_t generation, bool success, qint64 size);

        AbstractRemoteFolderProvider &_remoteFolderProvider;
        std::unique_ptr<TreeNode> _root{std::make_unique<TreeNode>()};
        QHash<QString, TreeNode *> _nodesById;
        QSet<QString> _excludedNodeIds;
        QHash<QString, QString> _excludedPaths;
        QQueue<QString> _sizeQueue;
        UserDbId _userDbId{0};
        DriveId _driveId{0};
        uint64_t _generation{0};
        uint8_t _activeSizeRequests{0};
        uint32_t _pendingInitialPathRequests{0};
        InitialPathsState _initialPathsState{InitialPathsState::Ready};
        bool _initialPathRequestFailed{false};
};

} // namespace KDC
