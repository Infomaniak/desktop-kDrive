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
#include "db/parmsdb.h"
#include "db/sync.h"
#include "log/log.h"
#include "utility/utility.h"

#include <iostream>
#include <log4cplus/loggingmacros.h>

namespace KDC {

void NavigationPaneHelper::showInExplorerNavigationPane() {
    // TODO: Remove debug logs once blocking issue is resolved
    std::cout << "[DEBUG] NavigationPaneHelper::showInExplorerNavigationPane - START" << std::endl;

    // Fix folders without CLSID (to remove later)
    std::cout << "[DEBUG] NavigationPaneHelper - Selecting all syncs from DB" << std::endl;
    std::vector<KDC::Sync> syncList;
    if (!KDC::ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        std::cout << "[DEBUG] NavigationPaneHelper - Failed to select syncs" << std::endl;
        return;
    }
    std::cout << "[DEBUG] NavigationPaneHelper - Found " << syncList.size() << " syncs" << std::endl;

    std::cout << "[DEBUG] NavigationPaneHelper - Updating cloud storage registry" << std::endl;
    updateCloudStorageRegistry();
    std::cout << "[DEBUG] NavigationPaneHelper - Cloud storage registry updated" << std::endl;

    // Set pin state
    std::cout << "[DEBUG] NavigationPaneHelper - Setting pin state for syncs" << std::endl;
    for (const Sync &sync: syncList) {
        std::cout << "[DEBUG] NavigationPaneHelper - Setting pin state for sync: " << sync.dbId() << std::endl;
        Utility::setFolderPinState(CommonUtility::s2ws(sync.navigationPaneClsid()), true);
    }
    std::cout << "[DEBUG] NavigationPaneHelper::showInExplorerNavigationPane - END" << std::endl;
}

void NavigationPaneHelper::updateCloudStorageRegistry() {
    // TODO: Remove debug logs once blocking issue is resolved
    std::cout << "[DEBUG] NavigationPaneHelper::updateCloudStorageRegistry - START" << std::endl;

    // Start by looking at every registered namespace extension for the sidebar, and look for an "ApplicationName" value
    // that matches ours when we saved.
    std::cout << "[DEBUG] NavigationPaneHelper - Walking registry subkeys" << std::endl;
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
    std::cout << "[DEBUG] NavigationPaneHelper - Registry walk completed, found " << entriesToRemove.size()
              << " entries to remove" << std::endl;

    // Then remove anything
    std::cout << "[DEBUG] NavigationPaneHelper - Removing legacy sync root keys" << std::endl;
    foreach (auto &clsid, entriesToRemove) {
        Utility::removeLegacySyncRootKeys(clsid);
    }
    std::cout << "[DEBUG] NavigationPaneHelper - Legacy sync root keys removed" << std::endl;

    // Then re-save every folder that has a valid navigationPaneClsid to the registry
    std::cout << "[DEBUG] NavigationPaneHelper - Re-saving folder registry entries" << std::endl;
    std::vector<KDC::Sync> syncList;
    if (!KDC::ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        std::cout << "[DEBUG] NavigationPaneHelper - Failed to select syncs" << std::endl;
        return;
    }

    std::cout << "[DEBUG] NavigationPaneHelper - Adding legacy sync root keys for " << syncList.size() << " syncs"
              << std::endl;
    for (KDC::Sync &sync: syncList) {
        if (sync.virtualFileMode() != KDC::VirtualFileMode::Win) {
            std::cout << "[DEBUG] NavigationPaneHelper - Adding keys for sync: " << sync.dbId() << std::endl;
            Utility::addLegacySyncRootKeys(CommonUtility::s2ws(sync.navigationPaneClsid()), sync.localPath(), true);
        }
    }
    std::cout << "[DEBUG] NavigationPaneHelper::updateCloudStorageRegistry - END" << std::endl;
}

} // namespace KDC
