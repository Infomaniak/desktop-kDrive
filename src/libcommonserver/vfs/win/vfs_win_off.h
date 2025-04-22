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
#include "libcommonserver/vfs/vfswin.h"
namespace KDC {

class SYNCENGINEVFS_EXPORT VfsWinOff : public VfsWin {
        Q_OBJECT
        Q_INTERFACES(KDC::VfsWin)

    public:
        using VfsWinOff::VfsWin;
        ExitInfo createPlaceholder(const SyncPath &relativeLocalPath, const SyncFileItem &item) override;
        ExitInfo convertToPlaceholder(const SyncPath &path, const SyncFileItem &item) override;

};

class WinVfsPluginFactory : public QObject, public DefaultPluginFactory<VfsWin> {
        Q_OBJECT
        Q_PLUGIN_METADATA(IID "org.kdrive.PluginFactory" FILE "../vfspluginmetadata.json")
        Q_INTERFACES(KDC::PluginFactory)
};

} // namespace KDC
