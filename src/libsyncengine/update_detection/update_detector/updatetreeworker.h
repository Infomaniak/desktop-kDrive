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

#include "syncpal/isyncworker.h"
#include "syncpal/syncpal.h"
#include "db/syncdb.h"
#include "update_detection/file_system_observer/fsoperationset.h"
#include "updatetree.h"

namespace KDC {

class UpdateTreeWorker;

typedef ExitCode (UpdateTreeWorker::*stepptr)();

class UpdateTreeWorker : public ISyncWorker {
    public:
        struct IntegrityError : std::runtime_error {
                using std::runtime_error::runtime_error;
        };

        UpdateTreeWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName,
                         ReplicaSide side);
        UpdateTreeWorker(SyncDbReadOnlyCache &syncDbReadOnlyCache, std::shared_ptr<FSOperationSet> operationSet,
                         std::shared_ptr<UpdateTree> updateTree, const std::string &name, const std::string &shortName,
                         ReplicaSide side);
        ~UpdateTreeWorker();

        void execute() override;

    private:
        SyncDbReadOnlyCache &_syncDbReadOnlyCache;
        std::shared_ptr<FSOperationSet> _operationSet;
        std::shared_ptr<UpdateTree> _updateTree;
        using FSOpPtrMap = std::unordered_map<SyncPath, FSOpPtr, PathHashFunction>;
        FSOpPtrMap _createFileOperationSet;
        ReplicaSide _side;

        /**
         * Create node where opType is Move
         * and nodeType is Directory.
         * return : ExitCode::Ok if task is successful.
         */
        ExitCode step1MoveDirectory();

        /**
         * Create node where opType is Move
         * and nodeType is File.
         * return : ExitCode::Ok if task is successful.
         */
        ExitCode step2MoveFile();

        /**
         * Create node where opType is Delete
         * and nodeType is Directory.
         * return : ExitCode::Ok if task is successful.
         */
        ExitCode step3DeleteDirectory();

        /**
         * Create node where opType is Delete
         * and nodeType is File.
         * return : ExitCode::Ok if task is successful.
         */
        ExitCode step4DeleteFile();

        /**
         * Create node where opType is Create
         * and nodeType is Directory.
         * return : ExitCode::Ok if task is successful.
         */
        ExitCode step5CreateDirectory();

        /**
         * Create node where opType is Create
         * and nodeType is File.
         * return : ExitCode::Ok if task is successful.
         */
        ExitCode step6CreateFile();

        /**
         * Create node where opType is Edit
         * and nodeType is File.
         * return : ExitCode::Ok if task is successful.
         */
        ExitCode step7EditFile();

        /**
         * Update existing node with information from DB
         * and add missing nodes without change events.
         * return : ExitCode::Ok if task is successful.
         */
        ExitCode step8CompleteUpdateTree();

        ExitCode createMoveNodes(const NodeType &nodeType);

        ExitCode getNewPathAfterMove(const SyncPath &path, SyncPath &newPath);
        ExitCode updateNodeWithDb(const std::shared_ptr<Node> parentNode);
        [[nodiscard]] ExitCode mergeNodeToParentChildren(std::shared_ptr<Node> parentNode, const std::shared_ptr<Node> node);
        ExitCode tryToMergeTmpNode(const std::shared_ptr<Node> tmpNode, bool &merged);
        ExitCode updateTmpNode(const std::shared_ptr<Node> tmpNode);
        ExitCode getOriginPath(const std::shared_ptr<Node> node, SyncPath &path);

        // Log update information if extended logging is on.
        void logUpdate(const std::shared_ptr<Node> node, const OperationType opType,
                       const std::shared_ptr<Node> parentNode = nullptr);
        [[nodiscard]] bool updateTmpFileNode(const std::shared_ptr<Node> node, FSOpPtr op, FSOpPtr deleteOp,
                                             OperationType opType);
        /**
         * Search for the nearest ancestor of the node with path `nodePath` in the update tree through its database ID.
         \param nodePath: the path of the node whose ancestor is queried
         \param ancestorNode: will be set with a pointer to the ancestor node.
         \return : ExitCode::Ok if no unexpected error occurred.
         */
        ExitCode searchForAncestorNode(const SyncPath &nodePath, std::shared_ptr<Node> &ancestorNode);

        /**
         * Detect and handle create operations on files or directories
         * with identical standardized paths.
         * The existence of such duplicate standardized paths can be caused by:
         * - a file deletion operation was not reported by the user OS.
         * - the user has created several files whose names have different encodings but same normalization (an issue
         *reported on Windows 10 and 11). This function fills `_createFileOperation` with all create operations on files.
         *
         *\return : ExitCode::Ok if no problematic create operations were detected.
         */
        ExitCode handleCreateOperationsWithSamePath();

        /**
         * Attempt to merge a Delete operation with the Create operation of an item reusing the same path, so that both
         * operations are processed as a single Edit operation.
         * Some software, such as Excel, keeps the current version into a temporary directory and moves it to the
         * destination, replacing the original file. However, this behavior is applied on the local replica only.
         * The merge is not performed if the type of the item has changed, as the API does not support editing an item
         * while changing its type (see checkIfDeleteAndCreateCanBeMerged).
         * \param deleteOp: the Delete operation to be merged with a Create operation.
         * \param op: the operation to be processed. Set to the Create operation if the merge is performed, otherwise
         *            left unchanged.
         * \param skipOp: set to true if the Create operation is no longer present in the operation set, in which case
         *                the Delete operation must be ignored.
         * \return : ExitCode::Ok if no unexpected error occurred.
         */
        [[nodiscard]] ExitCode mergeWithCreateOperationIfApplicable(const FSOpPtr &deleteOp, FSOpPtr &op, bool &skipOp);

        /**
         * Check if a Delete operation and a Create operation, on an item reusing the same path, can be merged into a
         * single Edit operation.
         * The merge is not possible if the type of the item has changed (e.g., a symbolic link replaced by a regular
         * file), as the API does not support editing an item while changing its type. In that case, the two operations
         * are kept separate, so that the corresponding remote item is deleted and re-created.
         * A symbolic link is stored as a File node, both in the snapshots and in the database. A change of type
         * (e.g., a symbolic link replaced by a regular file) can therefore only be detected by comparing the link
         * flags of the two items.
         * \param deleteOp: the Delete operation of the deleted item.
         * \param createOp: the Create operation of the newly created item.
         * \param canMerge: will be set to true if the two operations can be merged into a single Edit operation.
         * \return : ExitCode::Ok if no unexpected error occurred.
         */
        [[nodiscard]] ExitCode checkIfDeleteAndCreateCanBeMerged(const FSOpPtr &deleteOp, const FSOpPtr &createOp,
                                                                 bool &canMerge);

        [[nodiscard]] ExitCode getOrCreateNodeFromPath(const SyncPath &path, std::shared_ptr<Node> &node);
        [[nodiscard]] ExitCode createTmpNode(std::shared_ptr<Node> &tmpNode, const SyncName &name,
                                             const std::shared_ptr<Node> parentNode);

        /**
         * Create missing nodes in the update tree for a given 'path'.
         \param path: the given path for which missing nodes should be created in the update tree.
         \param ancestorNode: the nearest ancestor of the node with path `path` in the update tree.
         \param parentNode: will be set with a pointer to the node corresponding to the parent of the `path`.
         \return : ExitCode::Ok if no unexpected error occurred.
         */
        [[nodiscard]] ExitCode createMissingNodesFromPath(const SyncPath &path, const std::shared_ptr<Node> ancestorNode,
                                                          std::shared_ptr<Node> &parentNode);

        /**
         * Create missing nodes in the update tree for a deleted item.
         \param path: the given path for which missing nodes should be created in the update tree.
         \param createPath: in the case of a Create-Delete, the path of the corresponding Create operation.
         \param parentNode: will be set with a pointer to the node corresponding to the parent of the `path`.
         \return : ExitCode::Ok if no unexpected error occurred.
         */
        [[nodiscard]] ExitCode createMissingNodesForDeletedItem(const SyncPath &path, const SyncPath &createPath,
                                                                std::shared_ptr<Node> &parentNode);
        [[nodiscard]] ExitCode createMissingNodesForDeletedItem(const SyncPath &path, std::shared_ptr<Node> &parentNode);

        /**
         * @brief This method gets a node from a deleted path recursively. Recursion here ensures that, even if 2 branches have
         * nodes with the same names, the deleted branch is retrieved (see integration tests on branches deleted and re-created).
         * @param path The path of the node to be retrieved.
         * @return A shared pointer to the node. If the node is not found, the shared pointer is a `nullptr`.
         */
        [[nodiscard]] std::shared_ptr<Node> getNodeFromDeletedPath(const SyncPath &path);

        bool mergingTempNodeToRealNode(std::shared_ptr<Node> tmpNode, std::shared_ptr<Node> realNode);

        /**
         * Check that there is no temporary node remaining in the update tree
         * @return true if no temporary node is found
         */
        bool checkTreeIntegrity();
        bool checkNodeIntegrity(const std::shared_ptr<Node> node);
        bool checkOperationTypes(const std::shared_ptr<Node> node);

        bool resetNodes();

        friend class TestUpdateTreeWorker;
};

} // namespace KDC
