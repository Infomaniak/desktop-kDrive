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
        ExitCode updateTmpNode(const std::shared_ptr<Node> tmpNode);
        ExitCode getOriginPath(const std::shared_ptr<Node> node, SyncPath &path);

        // Log update information if extended logging is on.
        void logUpdate(const std::shared_ptr<Node> node, const OperationType opType,
                       const std::shared_ptr<Node> parentNode = nullptr);
        [[nodiscard]] bool updateTmpFileNode(const std::shared_ptr<Node> node, FSOpPtr op, FSOpPtr deleteOp,
                                             OperationType opType);
        /**
         * Search for the parent of the node with path `nodePath` in the update tree through its database ID.
         \param nodePath: the path of the node whose parent is queried
         \param parentNode: it is set with a pointer to the parent node if it exists, with `nullptr` otherwise.
         \return : ExitCode::Ok if no unexpected error occurred.
         */
        ExitCode searchForParentNode(const SyncPath &nodePath, std::shared_ptr<Node> &parentNode);

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

        [[nodiscard]] ExitCode getOrCreateNodeFromPath(const SyncPath &path, std::shared_ptr<Node> &node,
                                                       bool existingBranchOnly = true);
        [[nodiscard]] ExitCode createTmpNode(std::shared_ptr<Node> &tmpNode, const SyncName &name,
                                             const std::shared_ptr<Node> &parentNode);
        [[nodiscard]] ExitCode getOrCreateNodeFromExistingPath(const SyncPath &path, std::shared_ptr<Node> &node) {
            return getOrCreateNodeFromPath(path, node, true);
        }
        /**
         * @brief This method gets a node from a deleted path recursively. Recursion here ensures that, even if 2 branches have
         * nodes with the same names, the deleted branch is retrieved.
         * @param path The path of the node to be retrieved.
         * @return A shared pointer to the node. If the node is not found, the shared pointer is a `nullptr`.
         */
        [[nodiscard]] std::shared_ptr<Node> getNodeFromDeletedPath(const SyncPath &path);

        bool mergingTempNodeToRealNode(std::shared_ptr<Node> tmpNode, std::shared_ptr<Node> realNode);

        /**
         * Check that there is no temporary node remaining in the update tree
         * @return true if no temporary node is found
         */
        bool integrityCheck();

        friend class TestUpdateTreeWorker;
};

} // namespace KDC
