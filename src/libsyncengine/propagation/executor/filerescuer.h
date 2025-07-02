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
#include "libsyncengine/syncpal/syncpal.h"

namespace KDC {

class FileRescuer {
    public:
        explicit FileRescuer(std::shared_ptr<SyncPal> syncPal) :
            _syncPal(syncPal) {}

        ExitInfo executeRescueMoveJob(SyncOpPtr syncOp);

        static SyncPath rescueFolderName() { return _rescueFolderName; }

    private:
        ExitInfo createRescueFolderIfNeeded() const;
        ExitInfo moveToRescueFolder(const SyncPath &relativeOriginPath, SyncPath &relativeDestinationPath) const;
        SyncPath getDestinationPath(const SyncPath &relativeOriginPath, uint16_t counter = 0) const;

        std::shared_ptr<SyncPal> _syncPal;
        static const SyncPath _rescueFolderName;

        friend class TestFileRescuer;
};

} // namespace KDC
