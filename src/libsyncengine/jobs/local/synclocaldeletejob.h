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

#include "genericlocaldeletejob.h"

#include "syncpal/syncpal.h"

namespace KDC {

/**
 * @brief Delete job designed to delete a local file in the context of synchronization.
 */
class SyncLocalDeleteJob : public GenericLocalDeleteJob {
    public:
        SyncLocalDeleteJob(const std::shared_ptr<SyncPal> syncPal, const SyncPath &relativePath, bool liteSyncIsEnabled,
                           const NodeId &remoteId,
                           bool forceToTrash = false); // Check existence of remote counterpart and abort if needed.
        SyncLocalDeleteJob(const std::shared_ptr<SyncPal> syncPal, const SyncPath &absolutePath); // Delete without checks

    protected:
        bool _liteSyncIsEnabled = false;

        ExitInfo canRun() override;
        virtual bool findRemoteItem(SyncPath &remoteItemPath) const;
        virtual ExitInfo moveToTrash();

    private:
        ExitInfo runJob() override;

        ExitInfo hardDeleteDehydratedPlaceholders();
        ExitInfo handleLiteSyncFile(const SyncPath &path);
        ExitInfo deleteFromDB(const SyncPath &path);

        //! Returns `true` if `localRelativePath` and `remoteRelativePath` indicate the same synchronised item.
        /*!
          \param targetPath is the path of the remote sync folder if the sync is advanced; it is empty otherwise.
          \param localRelativePath is the path of the item relative to the local sync folder.
          \param remoteRelativePath is the path of the item relative to the remote drive root folder.
          \return true if the two relative paths indicate the same synchronised item.
        */
        bool matchRelativePaths(const SyncPath &targetPath, const SyncPath &localRelativePath,
                                const SyncPath &remoteRelativePath);

        ExitInfo checkIfRemoteFileHasBeenMoved();

        const std::shared_ptr<SyncPal> _syncPal;
        SyncPath _relativeLocalPath;
        NodeId _remoteNodeId;
        bool _forceToTrash = false;

        struct Path {
                Path(const SyncPath &path);
                bool endsWith(SyncPath &&ending) const;

            private:
                SyncPath _path;
        };

        friend class TestLocalJobs;
};

} // namespace KDC
