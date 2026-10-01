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

#include "libcommon/info/nodeinfo.h"
#include "libcommon/utility/types.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QSet>
#include <QTimer>

#include <memory>
#include <unordered_set>
#include <vector>

namespace KDC {

class AbstractRemoteFolderProvider;

/**
 * Lazy remote-folder tree used to choose one destination folder on a drive.
 *
 * Role: present the drive root as a single top-level row, load its sub-folders on demand, and keep one selected folder.
 * The drive root itself, folders the user cannot open, and folders the caller marks as unavailable cannot be selected. The
 * model also hosts the editing row of an inline folder creation, but never creates a folder itself: its owner sends the
 * request, then reports the pending state and the created folder back.
 */
class RemoteFolderPickerModel final : public QAbstractItemModel {
        Q_OBJECT
        Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
        Q_PROPERTY(bool loadFailed READ loadFailed NOTIFY stateChanged)
        Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
        Q_PROPERTY(QString selectedNodeId READ selectedNodeId NOTIFY selectionChanged)
        Q_PROPERTY(QString selectedName READ selectedName NOTIFY selectionChanged)
        Q_PROPERTY(QString selectedPath READ selectedPath NOTIFY selectionChanged)
        Q_PROPERTY(bool folderCreationActive READ folderCreationActive NOTIFY folderCreationChanged)
        Q_PROPERTY(bool folderCreationPending READ folderCreationPending NOTIFY folderCreationChanged)

    public:
        // Role names avoid `selected` and `editing`, which TableView already sets on its delegates from its own selection
        // and cell editing.
        enum Role : int32_t {
            NameRole = Qt::UserRole + 1,
            NodeIdRole,
            DriveRootRole,
            SelectableRole,
            CanCreateFolderRole,
            UnavailableRole,
            AccessDeniedRole,
            SelectedRole,
            ChildrenLoadingRole,
            ChildrenLoadFailedRole,
            EditingRole,
        };
        Q_ENUM(Role)

        explicit RemoteFolderPickerModel(AbstractRemoteFolderProvider &remoteFolderProvider, QObject *parent = nullptr);

        [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex &parentIndex = QModelIndex()) const override;
        [[nodiscard]] QModelIndex parent(const QModelIndex &child) const override;
        [[nodiscard]] int rowCount(const QModelIndex &parentIndex = QModelIndex()) const override;
        [[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
        [[nodiscard]] bool hasChildren(const QModelIndex &parentIndex = QModelIndex()) const override;
        [[nodiscard]] bool canFetchMore(const QModelIndex &parentIndex) const override;
        void fetchMore(const QModelIndex &parentIndex) override;

        // Report the loading state of the drive root's sub-folders.
        [[nodiscard]] bool loading() const;
        [[nodiscard]] bool loadFailed() const;
        [[nodiscard]] bool hasSelection() const { return !_selectedNodeId.isEmpty(); }
        [[nodiscard]] QString selectedNodeId() const { return _selectedNodeId; }
        [[nodiscard]] QString selectedName() const { return _selectedName; }
        // The remote path of the selected folder, as the server returned it.
        [[nodiscard]] QString selectedPath() const { return _selectedPath; }
        [[nodiscard]] bool folderCreationActive() const { return _editingNode != nullptr; }
        [[nodiscard]] bool folderCreationPending() const { return _editingNode && _editingNode->creating; }
        // Node id of the folder under which the editing row lives, empty when no creation is active.
        [[nodiscard]] QString folderCreationParentNodeId() const;
        // Remote path of that folder, empty for the drive root.
        [[nodiscard]] QString folderCreationParentPath() const;

        /**
         * Resets the tree to the given drive and starts loading its root.
         *
         * `unavailableNodeIds` lists folders that cannot be chosen, for instance because another synchronization already
         * targets them. A folder can still be expanded to reach its sub-folders.
         */
        void configure(UserDbId userDbId, DriveId driveId, const QString &driveName,
                       const std::unordered_set<NodeId> &unavailableNodeIds);
        /** Empties the tree without loading anything, and drops the responses still in flight. */
        void reset();
        /**
         * Restores a previously chosen folder without requiring it to be loaded, so that reopening the picker keeps the
         * current choice. The row shows its selection once its branch is loaded.
         */
        void restoreSelection(const NodeId &nodeId, const QString &name, const QString &path);

        /** Marks the editing row as waiting for the owner's creation request, which makes it read-only. */
        void setFolderCreationPending(bool pending);
        /** Replaces the editing row with the created folder and selects it. */
        void insertCreatedFolder(const NodeInfo &info);

        /** Parent of a row, for keyboard navigation: QAbstractItemModel::parent() is not callable from QML. */
        Q_INVOKABLE [[nodiscard]] QModelIndex parentIndex(const QModelIndex &modelIndex) const { return parent(modelIndex); }
        /** Node id of a row, empty for an invalid index or the editing row, so the view can follow a row by its folder. */
        Q_INVOKABLE [[nodiscard]] QString nodeIdAt(const QModelIndex &modelIndex) const;

        Q_INVOKABLE void retryRoot();
        Q_INVOKABLE void retryChildren(const QModelIndex &modelIndex);
        /**
         * Selects the folder when it can be chosen, and reports whether the selection now designates it. Choosing a row that
         * cannot be selected, such as the drive root, clears the selection instead, so the choice is never left on a folder
         * the user moved away from.
         */
        Q_INVOKABLE bool select(const QModelIndex &modelIndex);
        /**
         * Reports that the folder with the given node id entered or left the viewport.
         *
         * A folder that stays visible loads its immediate children, so its expand affordance reflects whether it really has
         * sub-folders. The request waits for the view to settle: folders that only pass through the viewport while the user
         * scrolls are never listed. The folder is named by its node id rather than by an index: a view row can show another
         * folder after an expansion without being recreated. An unknown id, from a previous configuration, is ignored.
         */
        Q_INVOKABLE void setNodeVisible(const QString &nodeId, bool visible);
        /**
         * Opens an editing row as the first child of the given folder, loading its children first when needed. A previous
         * editing row that is not being created is dropped. Refused while a creation request is pending, and below a folder
         * that cannot be opened or chosen.
         */
        Q_INVOKABLE bool beginFolderCreation(const QModelIndex &parentIndex);
        /** Drops the editing row, unless its creation request is pending. */
        Q_INVOKABLE void cancelFolderCreation();

    signals:
        void stateChanged();
        void selectionChanged();
        void folderCreationChanged();
        // The editing row was inserted below `parentIndex`, which the view expands before focusing the row.
        void folderCreationStarted(const QModelIndex &parentIndex);
        // The created folder was inserted at its sorted position, which the view scrolls to.
        void folderCreated(const QModelIndex &index);

    private:
        enum class LoadState : uint8_t {
            NotLoaded,
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
                bool accessDenied{false};
                bool driveRoot{false};
                bool editing{false};
                bool creating{false};
        };

        void clearTree();
        [[nodiscard]] TreeNode *nodeForIndex(const QModelIndex &modelIndex) const;
        [[nodiscard]] QModelIndex indexForNode(const TreeNode *node) const;
        [[nodiscard]] TreeNode *driveRootNode() const;
        [[nodiscard]] bool isSelectable(const TreeNode *node) const;
        [[nodiscard]] bool canCreateFolder(const TreeNode *node) const;
        void requestChildren(TreeNode *node);
        void prefetchVisibleChildren();
        void handleChildrenResult(TreeNode *node, uint64_t generation, bool success, const std::vector<NodeInfo> &children);
        void notifyChildrenState(const TreeNode *node);
        void insertEditingRow(TreeNode *parentNode);
        void removeEditingRow();
        [[nodiscard]] std::size_t sortedInsertionRow(const TreeNode *parentNode, const QString &name) const;
        void setSelection(const QString &nodeId, const QString &name, const QString &path);
        void notifySelectedRow(const QString &nodeId);

        AbstractRemoteFolderProvider &_remoteFolderProvider;
        // Invisible model root; its only child is the drive root row.
        std::unique_ptr<TreeNode> _root{std::make_unique<TreeNode>()};
        QHash<QString, TreeNode *> _nodesById;
        QSet<QString> _unavailableNodeIds;
        TreeNode *_editingNode{nullptr};
        // Folder whose children are loading before its editing row can be inserted.
        QString _pendingCreationParentNodeId;
        QString _selectedNodeId;
        QString _selectedName;
        QString _selectedPath;
        UserDbId _userDbId{0};
        DriveId _driveId{0};
        uint64_t _generation{0};
        // Visible folders whose children are listed once the view has settled.
        QSet<QString> _childrenPrefetchCandidates;
        QTimer _childrenPrefetchTimer;
};

} // namespace KDC
