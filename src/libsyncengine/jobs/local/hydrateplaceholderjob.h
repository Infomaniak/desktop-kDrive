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

#include "jobs/syncjob.h"
#include "syncpal/syncpal.h"

namespace KDC {

//! Local job that hydrates the placeholder of the item at a given path.
/*!
 * The job checks that the item is a hydratable placeholder (i.e. neither a symlink nor an already hydrated or syncing
 * placeholder), sets its pin state to PinState::AlwaysLocal (macOS), and enqueues the download job that will retrieve
 * the item content. The logic reproduces the behavior of ExtensionJob::commandMakeAvailableLocallyDirect for a single
 * item.
 */
class HydratePlaceHolderJob : public SyncJob {
    public:
        //! \param parentFolderPath is the absolute path of the parent folder whose hydration is monitored, empty otherwise.
        HydratePlaceHolderJob(std::shared_ptr<SyncPal> syncPal, SyncPath absoluteLocalPath, SyncPath relativePath,
                              SyncPath parentFolderPath = {});

    protected:
        ExitInfo canRun() override;

    private:
        ExitInfo runJob() override;

        //! Checks the status of the item. Reproduces the behavior of ExtensionJob::syncFileStatus without the server-side
        //! maps.
        ExitInfo syncFileStatus(SyncFileStatus &status, VfsStatus &vfsStatus);
        //! Cancels an ongoing hydration of the item and restores `pinState` if it is not PinState::Unknown.
        ExitInfo cancelHydrate(PinState pinState, ExitInfo exitInfo);

        const std::shared_ptr<SyncPal> _syncPal;
        const SyncPath _absoluteLocalPath;
        const SyncPath _relativePath;
        const SyncPath _parentFolderPath;
};

} // namespace KDC
