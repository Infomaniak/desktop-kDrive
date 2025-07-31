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

#include "navigationpanehelper.h"
#include "config.h"
#include "libparms/db/parmsdb.h"
#include "libparms/db/parameters.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "libsyncengine/requests/parameterscache.h"
#include "libparms/db/parmsdb.h"

#include <QUuid>

#include <log4cplus/loggingmacros.h>

namespace KDC {

NavigationPaneHelper::NavigationPaneHelper(const std::unordered_map<int, std::shared_ptr<KDC::Vfs>> &vfsMap) :
    _vfsMap(vfsMap) {
    _showInExplorerNavigationPane = KDC::ParametersCache::instance()->parameters().showShortcuts();

#ifdef Q_OS_WIN
    _updateCloudStorageRegistryTimer.setSingleShot(true);
    connect(&_updateCloudStorageRegistryTimer, &QTimer::timeout, this, &NavigationPaneHelper::updateCloudStorageRegistry);
#endif
}

#ifdef Q_OS_WIN
void NavigationPaneHelper::setShowInExplorerNavigationPane(bool show) {
    _showInExplorerNavigationPane = show;

    // Fix folders without CLSID (to remove later)
    std::vector<KDC::Sync> syncList;
    if (!KDC::ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        return;
    }

    scheduleUpdateCloudStorageRegistry();

    // Set pin state
    for (const Sync &sync: syncList) {
        Utility::setFolderPinState(CommonUtility::s2ws(sync.navigationPaneClsid()), show);
    }
}

void NavigationPaneHelper::scheduleUpdateCloudStorageRegistry() {
    // Schedule the update to happen a bit later to avoid doing the update multiple times in a row.
    if (!_updateCloudStorageRegistryTimer.isActive()) _updateCloudStorageRegistryTimer.start(500);
}

void NavigationPaneHelper::updateCloudStorageRegistry() {
    // Start by looking at every registered namespace extension for the sidebar, and look for an "ApplicationName" value
    // that matches ours when we saved.
    std::vector<std::wstring> entriesToRemove;
    (void) Utility::registryWalkSubKeys(HKEY_CURRENT_USER,
                                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Desktop\\NameSpace",
                                        [&entriesToRemove](const HKEY key, const std::wstring &subKey) {
                                            auto appName = Utility::registryGetKeyValue(key, subKey, L"ApplicationName");
                                            try {
                                                if (std::get<std::wstring>(appName) == CommonUtility::s2ws(APPLICATION_NAME)) {
                                                    (void) entriesToRemove.emplace_back(subKey);
                                                }
                                            } catch (const std::bad_variant_access &) {
                                                // Nothing to do, the key simply does not exist.
                                            }
                                        });

    // Then remove anything
    foreach (auto &clsid, entriesToRemove) {
        Utility::removeLegacySyncRootKeys(clsid);
    }

    // Then re-save every folder that has a valid navigationPaneClsid to the registry
    std::vector<KDC::Sync> syncList;
    if (!KDC::ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        return;
    }

    for (KDC::Sync &sync: syncList) {
        if (sync.virtualFileMode() != KDC::VirtualFileMode::Win) {
            if (sync.navigationPaneClsid().empty()) {
                sync.setNavigationPaneClsid(QUuid::createUuid().toString().toStdString());
            }
            Utility::addLegacySyncRootKeys(CommonUtility::s2ws(sync.navigationPaneClsid()), sync.localPath(),
                                           _showInExplorerNavigationPane);
        }
    }
}
#endif

} // namespace KDC
