/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "libcommonserver/vfs.h"

#include <QObject>
#include <QTimer>
#include <unordered_map>

namespace KDC {

class NavigationPaneHelper : public QObject {
        Q_OBJECT
    public:
        NavigationPaneHelper(const std::unordered_map<int, std::shared_ptr<KDC::Vfs>> &vfsMap);

#ifdef Q_OS_WIN
        bool showInExplorerNavigationPane() const { return _showInExplorerNavigationPane; }
        void setShowInExplorerNavigationPane(bool show);

        void scheduleUpdateCloudStorageRegistry();
#endif

    private:
        const std::unordered_map<int, std::shared_ptr<KDC::Vfs>> &_vfsMap;

#ifdef Q_OS_WIN
        void updateCloudStorageRegistry();
#endif

        bool _showInExplorerNavigationPane;
#ifdef Q_OS_WIN
        QTimer _updateCloudStorageRegistryTimer;
#endif
};

}  // namespace KDC
