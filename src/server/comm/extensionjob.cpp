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

#include "extensionjob.h"
#include "commmanager.h"
#include "appserver.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility_base.h"
#include "libcommon/utility/utility.h"
#include "libcommon/theme/theme.h"
#include "libcommonserver/io/iohelper.h"
#include "libparms/db/parmsdb.h"

#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include <QClipboard>
#include <QBuffer>
#include <QDesktopServices>
#include <QPixmap>
#include <QUrl>

#if defined(KD_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace KDC {

bool syncForPath(const std::vector<Sync> &syncList, const SyncPath &path, Sync &sync) {
    for (const Sync &tmpSync: syncList) {
        if (CommonUtility::isSubDir(tmpSync.localPath(), path)) {
            sync = tmpSync;
            return true;
        }
    }

    return false;
}

bool syncForPath(const SyncPath &path, Sync &sync) {
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        return false;
    }

    return syncForPath(syncList, path, sync);
}

bool syncForPaths(const std::vector<SyncPath> &paths, Sync &sync) {
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        return false;
    }

    for (const auto &path: paths) {
        Sync tmpSync;
        if (!syncForPath(syncList, path, tmpSync)) {
            return false;
        }
        if (tmpSync.dbId() != sync.dbId()) {
            if (!sync.dbId()) {
                sync = tmpSync;
            } else {
                sync.setDbId(0);
                break;
            }
        }
    }

    return true;
}

bool ExtensionJob::_dehydrationCanceled = false;
unsigned ExtensionJob::_nbOfOngoingDehydration = 0;
std::mutex ExtensionJob::_dehydrationMutex;

ExtensionJob::ExtensionJob(std::shared_ptr<CommManager> commManager, const CommString &commandLineStr,
                           const std::list<std::shared_ptr<AbstractCommChannel>> &channels) :
    _commManager(commManager),
    _commandLineStr(commandLineStr),
    _channels(channels) {
    _commands = {{"REGISTER_PATH", std::bind_front(&ExtensionJob::commandRegisterFolder, this)},
                 {"UNREGISTER_PATH", std::bind_front(&ExtensionJob::commandUnregisterFolder, this)},
                 {"GET_STRINGS", std::bind_front(&ExtensionJob::commandGetStrings, this)},
                 {"STATUS", std::bind_front(&ExtensionJob::commandForceStatus, this)},
                 {"GET_MENU_ITEMS", std::bind_front(&ExtensionJob::commandGetMenuItems, this)},
                 {"COPY_PUBLIC_LINK", std::bind_front(&ExtensionJob::commandCopyPublicLink, this)},
                 {"COPY_PRIVATE_LINK", std::bind_front(&ExtensionJob::commandCopyPrivateLink, this)},
                 {"OPEN_PRIVATE_LINK", std::bind_front(&ExtensionJob::commandOpenPrivateLink, this)},
                 {"MAKE_AVAILABLE_LOCALLY_DIRECT", std::bind_front(&ExtensionJob::commandMakeAvailableLocallyDirect, this)},
                 {"RETRIEVE_FILE_STATUS", std::bind_front(&ExtensionJob::commandRetrieveFileStatus, this)},
#if defined(KD_WINDOWS)
                 {"GET_ALL_MENU_ITEMS", std::bind_front(&ExtensionJob::commandGetAllMenuItems, this)},
                 {"GET_THUMBNAIL", std::bind_front(&ExtensionJob::commandGetThumbnail, this)},
#endif
#if defined(KD_MACOS)
                 {"RETRIEVE_FOLDER_STATUS", std::bind_front(&ExtensionJob::commandRetrieveFolderStatus, this)},
                 {"MAKE_ONLINE_ONLY_DIRECT", std::bind_front(&ExtensionJob::commandMakeOnlineOnlyDirect, this)},
                 {"CANCEL_DEHYDRATION_DIRECT", std::bind_front(&ExtensionJob::commandCancelDehydrationDirect, this)},
                 {"CANCEL_HYDRATION_DIRECT", std::bind_front(&ExtensionJob::commandCancelHydrationDirect, this)},
                 {"SET_THUMBNAIL", std::bind_front(&ExtensionJob::commandSetThumbnail, this)}
#endif
    };
}

ExitInfo ExtensionJob::runJob() {
    if (_channels.empty()) {
        executeCommand(_commandLineStr, nullptr);
    } else {
        foreach (auto &channel, _channels) {
            executeCommand(_commandLineStr, channel);
        }
    }

    return ExitCode::Ok;
}

void ExtensionJob::commandGetMenuItems(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel) {
    const auto argumentList = CommonUtility::splitCommString(argument, messageArgSeparator);

    if (argumentList.empty()) {
        LOGW_WARN(Log::instance()->getLogger(), L"Invalid argument - arg=" << CommonUtility::commString2WStr(argument));
        return;
    }

    {
        CommString response(Str("GET_MENU_ITEMS"));
        response.append(responseToFinderArgSeparator);
        response.append(Str("BEGIN"));
        channel->sendMessage(response);
    }

    // Find the common sync
    std::vector<SyncPath> paths;
    (void) std::copy(argumentList.begin(), argumentList.end(), std::back_inserter(paths));

    Sync sync;
    if (syncForPaths(paths, sync) && sync.dbId()) {
        // Find SyncPal and Vfs associated to sync
        const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex, _commManager->appServer().vfsMapMutex);
        const auto syncPalMapIt = retrieveSyncPalMapIt(sync.dbId());
        const auto vfsMapIt = retrieveVfsMapIt(sync.dbId());

        if (syncPalMapIt != _commManager->appServer().syncPalMap.end() && syncPalMapIt->second &&
            vfsMapIt != _commManager->appServer().vfsMap.end() && vfsMapIt->second) {
            // Some options only show for single files
            bool isSingleFile = false;
            if (paths.size() == 1) {
                manageActionsOnSingleFile(channel, paths[0], syncPalMapIt, vfsMapIt, sync);
                isSingleFile = QFileInfo(CommonUtility::commString2QStr(paths[0])).isFile();
            }
#if defined(KD_MACOS)
            // File availability actions
            if (sync.virtualFileMode() != VirtualFileMode::Off && vfsMapIt->second->showPinStateActions()) {
                // Manage hydration/dehydration
                bool canCancelDehydration = false;

                {
                    const std::lock_guard lock2(_dehydrationMutex);
                    if (_nbOfOngoingDehydration > 0) {
                        canCancelDehydration = true;
                    }
                }

                bool canHydrate = true;
                bool canDehydrate = true;
                bool canCancelHydration = false;
                for (const auto &path: paths) {
                    VfsStatus vfsStatus;
                    if (!canCancelHydration && vfsMapIt->second->status(path, vfsStatus) && vfsStatus.isSyncing) {
                        canCancelHydration = syncPalMapIt->second->isDownloadOngoing(path);
                    }

                    if (isSingleFile) {
                        canHydrate = vfsStatus.isPlaceholder && !vfsStatus.isSyncing && !vfsStatus.isHydrated;
                        canDehydrate = vfsStatus.isPlaceholder && !vfsStatus.isSyncing && vfsStatus.isHydrated;
                    }
                }

                {
                    const std::scoped_lock lock2(_dehydrationMutex);
                    if (_nbOfOngoingDehydration > 0) {
                        canCancelDehydration = true;
                    }
                }

                // TODO: Should be a submenu, should use icons
                auto makePinContextMenu = [this, channel](bool makeAvailableLocally, bool freeSpace, bool cancelDehydration,
                                                          bool cancelHydration) {
                    buildAndSendMenuItemMessage(channel, Str("MAKE_AVAILABLE_LOCALLY_DIRECT"), makeAvailableLocally,
                                                vfsPinActionText());

                    if (cancelHydration) {
                        buildAndSendMenuItemMessage(channel, Str("CANCEL_HYDRATION_DIRECT"), true, cancelHydrationText());
                    }

                    buildAndSendMenuItemMessage(channel, Str("MAKE_ONLINE_ONLY_DIRECT"), freeSpace, vfsFreeSpaceActionText());

                    if (cancelDehydration) {
                        buildAndSendMenuItemMessage(channel, Str("CANCEL_DEHYDRATION_DIRECT"), true, cancelDehydrationText());
                    }
                };

                makePinContextMenu(canHydrate, canDehydrate, canCancelDehydration, canCancelHydration);
            }
#endif
        }
    }

    {
        CommString response(Str("GET_MENU_ITEMS"));
        response.append(responseToFinderArgSeparator);
        response.append(Str("END"));
        channel->sendMessage(response);
    }
}

void ExtensionJob::commandCopyPublicLink(const CommString &argument, std::shared_ptr<AbstractCommChannel>) {
    const auto fileData = FileData::get(argument);
    if (!fileData.isValid()) return;

    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    const auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _commManager->appServer().syncPalMap.end()) return;

    // Get NodeId
    NodeId nodeId;
    ExitCode exitCode = syncPalMapIt->second->fileRemoteIdFromLocalPath(fileData.relativePath, nodeId);
    if (exitCode != ExitCode::Ok) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in SyncPal::itemId - " << Utility::formatSyncPath(fileData.relativePath));
        return;
    }

    // Get public link URL
    std::string linkUrl;
    exitCode = _commManager->appServer().getPublicLinkUrl(fileData.driveDbId, nodeId, linkUrl);
    if (exitCode != ExitCode::Ok) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in getPublicLinkUrl - " << Utility::formatSyncPath(fileData.relativePath));
        return;
    }

    copyUrlToClipboard(linkUrl);
}

void ExtensionJob::commandCopyPrivateLink(const CommString &argument, std::shared_ptr<AbstractCommChannel>) {
    fetchPrivateLinkUrlHelper(argument, &ExtensionJob::copyUrlToClipboard);
}

void ExtensionJob::commandOpenPrivateLink(const CommString &argument, std::shared_ptr<AbstractCommChannel>) {
    fetchPrivateLinkUrlHelper(argument, &ExtensionJob::openPrivateLink);
}

void ExtensionJob::commandMakeAvailableLocallyDirect(const CommString &argument, std::shared_ptr<AbstractCommChannel>) {
    const auto fileList = CommonUtility::splitCommString(argument, messageArgSeparator);

    SyncPath parentFolder;
#if defined(KD_MACOS)
    if (fileList.size() == 1) {
        const auto fileData = FileData::get(fileList[0]);
        if (fileData.isDirectory) {
            monitorFolderHydration(fileData);
            parentFolder = fileData.localPath;
        }
    }

    std::vector<SyncPath> fileListExpanded;
    processFileList(fileList, fileListExpanded);

    for (const auto &filePath: fileListExpanded) {
#else
    for (const auto &filePathStr: fileList) {
        SyncPath filePath(filePathStr);
#endif
        auto fileData = FileData::get(filePath);
        if (!fileData.isValid()) {
            LOGW_WARN(Log::instance()->getLogger(), L"No file data - " << Utility::formatSyncPath(filePath));
            continue;
        }

        if (fileData.isLink) {
            LOGW_DEBUG(Log::instance()->getLogger(), L"Don't hydrate symlinks - " << Utility::formatSyncPath(filePath));
            continue;
        }

        // Check file status
        auto status = SyncFileStatus::Unknown;
        VfsStatus vfsStatus;
        if (!syncFileStatus(fileData, status, vfsStatus)) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in ExtensionJob::syncFileStatus - " << Utility::formatSyncPath(filePath));
            continue;
        }

        if (!vfsStatus.isPlaceholder) {
            // File is not a placeholder, this should never happen
            LOGW_WARN(Log::instance()->getLogger(), L"File is not a placeholder - " << Utility::formatSyncPath(filePath));
            continue;
        }

        if (vfsStatus.isHydrated || status == SyncFileStatus::Syncing) {
            LOGW_INFO(Log::instance()->getLogger(), L"File is already hydrated/ing - " << Utility::formatSyncPath(filePath));
            continue;
        }

#if defined(KD_MACOS)
        // Not done in Windows case: triggers an hydration
        // Get pin state
        PinState ps{PinState::Unknown};
        if (const auto exitInfo = getPinState(fileData, ps); !exitInfo) {
            LOGW_INFO(Log::instance()->getLogger(),
                      L"Error in ExtensionJob::pinState - " << Utility::formatSyncPath(filePath) << L": " << exitInfo);
            continue;
        }

        // Set pin state
        if (const auto exitInfo = setPinState(fileData, PinState::AlwaysLocal); !exitInfo) {
            LOGW_INFO(Log::instance()->getLogger(),
                      L"Error in ExtensionJob::setPinState - " << Utility::formatSyncPath(filePath) << L": " << exitInfo);
            continue;
        }
#endif

        if (!addDownloadJob(fileData, parentFolder)) {
            LOGW_INFO(Log::instance()->getLogger(),
                      L"Error in ExtensionJob::addDownloadJob - " << Utility::formatSyncPath(filePath));

#if defined(KD_MACOS)
            // Cancel hydration and reset pin state to initial value
            if (const auto exitInfo = cancelHydrate(fileData, ps); !exitInfo) {
                LOGW_INFO(Log::instance()->getLogger(),
                          L"Error in ExtensionJob::cancelHydrate - " << Utility::formatSyncPath(filePath) << L": " << exitInfo);
            }
#endif

            continue;
        }
    }
}

void ExtensionJob::commandRegisterFolder(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel) {
    if (!channel) return;

    CommString response(Str("REGISTER_PATH"));
    response.append(responseToFinderArgSeparator);
    response.append(argument); // path
    channel->sendMessage(response);
}

void ExtensionJob::commandUnregisterFolder(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel) {
    if (!channel) return;

    CommString response(Str("UNREGISTER_PATH"));
    response.append(responseToFinderArgSeparator);
    response.append(argument); // path
    channel->sendMessage(response);
}

void ExtensionJob::commandForceStatus(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel) {
    if (!channel) return;

    auto argumentList = CommonUtility::splitCommString(argument, messageArgSeparator);

    if (argumentList.size() != 4) {
        LOGW_WARN(Log::instance()->getLogger(), L"Invalid argument - arg=" << CommonUtility::commString2WStr(argument));
        return;
    }

    bool isSyncing(std::stoi(argumentList[0]));
    bool isHydrated(std::stoi(argumentList[2]));

    CommString status;
    if (isSyncing) {
        status.append(Str("SYNC_"));
        status.append(argumentList[1]); // progress
    } else if (isHydrated) {
        status = Str("OK");
    } else {
        status = Str("ONLINE");
    }

    CommString response(Str("STATUS"));
    response.append(responseToFinderArgSeparator);
    response.append(status);
    response.append(responseToFinderArgSeparator);
    response.append(argumentList[3]); // path
    channel->sendMessage(response);
}

void ExtensionJob::commandGetStrings(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel) {
    static std::array<std::pair<CommString, CommString>, 1> strings{
            {{Str("CONTEXT_MENU_TITLE"), CommonUtility::str2CommString(Theme::instance()->appName())}}};

    {
        CommString response(Str("GET_STRINGS"));
        response.append(responseToFinderArgSeparator);
        response.append(Str("BEGIN"));
        channel->sendMessage(response);
    }

    for (auto &[key, value]: strings) {
        if (argument.empty() || argument == key) {
            {
                CommString response(Str("STRING"));
                response.append(responseToFinderArgSeparator);
                response.append(key);
                response.append(responseToFinderArgSeparator);
                response.append(value);
                channel->sendMessage(response);
            }
        }
    }

    {
        CommString response(Str("GET_STRINGS"));
        response.append(responseToFinderArgSeparator);
        response.append(Str("END"));
        channel->sendMessage(response);
    }
}

void ExtensionJob::commandRetrieveFileStatus(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel) {
    const auto fileData = FileData::get(argument);
    auto status = SyncFileStatus::Unknown;
    VfsStatus vfsStatus;
    if (!syncFileStatus(fileData, status, vfsStatus)) {
        LOGW_DEBUG(Log::instance()->getLogger(),
                   L"Error in ExtensionJob::syncFileStatus - " << Utility::formatSyncPath(fileData.localPath));
        return;
    }

    const CommString message = buildMessage("STATUS", fileData.localPath, statusString(status, vfsStatus));
    channel->sendMessage(message);
}

#if defined(KD_WINDOWS)
void ExtensionJob::commandGetAllMenuItems(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel) {
    const auto argumentList = CommonUtility::splitCommString(argument, messageArgSeparator);

    if (argumentList.size() < 2) {
        LOGW_WARN(Log::instance()->getLogger(), L"Invalid argument - arg=" << CommonUtility::commString2WStr(argument));
        return;
    }


    CommString msgId = argumentList[0];
    CommString response(msgId);
    response.append(responseToFinderArgSeparator);
    response.append(CommonUtility::str2CommString(Theme::instance()->appName()));

    // Find the common sync
    std::vector<SyncPath> paths;
    (void) std::copy(argumentList.begin() + 1, argumentList.end(), std::back_inserter(paths));

    Sync sync;
    if (syncForPaths(paths, sync) && sync.dbId()) {
        const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex, _commManager->appServer().vfsMapMutex);
        const auto syncPalMapIt = retrieveSyncPalMapIt(sync.dbId());
        const auto vfsMapIt = retrieveVfsMapIt(sync.dbId());

        if (syncPalMapIt != _commManager->appServer().syncPalMap.end() && syncPalMapIt->second &&
            vfsMapIt != _commManager->appServer().vfsMap.end() && vfsMapIt->second) {
            response.append(responseToFinderArgSeparator);
            response.append(sync.dbId() ? Vfs::modeToString(vfsMapIt->second->mode()) : Str(""));

            // Some options only show for single files
            if (paths.size() == 1) {
                FileData fileData = FileData::get(paths[0]);
                NodeId nodeId;
                ExitCode exitCode = syncPalMapIt->second->fileRemoteIdFromLocalPath(fileData.relativePath, nodeId);
                if (exitCode != ExitCode::Ok) {
                    LOGW_WARN(Log::instance()->getLogger(),
                              L"Error in SyncPal::itemId - " << Utility::formatSyncPath(fileData.relativePath));
                    channel->sendMessage(response);
                    return;
                }
                bool isOnTheServer = !nodeId.empty();

                addSharingContextMenuOptions(fileData, response);
                response.append(responseToFinderArgSeparator);
                response.append(Str("OPEN_PRIVATE_LINK"));
                response.append(responseToFinderArgSeparator);
                response.append(isOnTheServer ? Str("") : Str("d"));
                response.append(responseToFinderArgSeparator);
                response.append(openInBrowserText());
            }

            // File availability actions
            bool canCancelHydration = false;
            if (sync.virtualFileMode() != VirtualFileMode::Off) {
                for (const auto &path: paths) {
                    auto fileData = FileData::get(path);
                    if (syncPalMapIt->second->isDownloadOngoing(fileData.relativePath)) {
                        canCancelHydration = true;
                        break;
                    }
                }
            }

            if (canCancelHydration) {
                response.append(responseToFinderArgSeparator);
                response.append(Str("CANCEL_HYDRATION_DIRECT"));
                response.append(responseToFinderArgSeparator);
                response.append(responseToFinderArgSeparator);
                response.append(cancelHydrationText());
            }
        }
    }
    channel->sendMessage(response);
}

void ExtensionJob::commandGetThumbnail(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel) {
    const auto argumentList = CommonUtility::splitCommString(argument, messageArgSeparator);

    if (argumentList.size() != 3) {
        LOGW_WARN(Log::instance()->getLogger(), L"Invalid argument - arg=" << CommonUtility::commString2WStr(argument));
        return;
    }

    // Msg Id
    CommString msgId(argumentList[0]);

    // Picture width asked
    unsigned int width(std::stoi(argumentList[1]));
    if (width == 0) {
        LOG_WARN(Log::instance()->getLogger(), "Bad width - value=" << width);
        return;
    }

    // File path
    SyncPath filePath(argumentList[2]);
    if (!QFileInfo(CommonUtility::commString2QStr(filePath)).isFile()) {
        LOGW_WARN(Log::instance()->getLogger(), L"Not a file - " << Utility::formatSyncPath(filePath));
        return;
    }

    FileData fileData = FileData::get(filePath);
    if (!fileData.isValid()) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"The file is not in a synchonized folder - " << Utility::formatSyncPath(filePath));
        return;
    }

    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    const auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _commManager->appServer().syncPalMap.end() || !syncPalMapIt->second) return;

    // Get NodeId
    NodeId nodeId;
    ExitCode exitCode = syncPalMapIt->second->fileRemoteIdFromLocalPath(fileData.relativePath, nodeId);
    if (exitCode != ExitCode::Ok) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in SyncPal::itemId - " << Utility::formatSyncPath(filePath));
        return;
    }

    // Get thumbnail
    std::string thumbnail;
    exitCode = _commManager->appServer().getThumbnail(fileData.driveDbId, nodeId, 256, thumbnail);
    if (exitCode != ExitCode::Ok) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in getThumbnail - " << Utility::formatSyncPath(filePath));
        return;
    }

    QByteArray thumbnailArr(thumbnail.c_str(), thumbnail.length());
    QPixmap pixmap;
    if (!pixmap.loadFromData(thumbnailArr)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in QPixmap::loadFromData - " << Utility::formatSyncPath(filePath));
        return;
    }

    // Resize pixmap
    LOG_DEBUG(Log::instance()->getLogger(), "Thumbnail fetched - size=" << pixmap.width() << "x" << pixmap.height());
    if (width) {
        if (pixmap.width() > pixmap.height()) {
            pixmap = pixmap.scaledToWidth(width, Qt::SmoothTransformation);
        } else {
            pixmap = pixmap.scaledToHeight(width, Qt::SmoothTransformation);
        }
        LOG_DEBUG(Log::instance()->getLogger(), "Thumbnail scaled - size=" << pixmap.width() << "x" << pixmap.height());
    }

    // Set Thumbnail
    QBuffer pixmapBuffer;
    pixmapBuffer.open(QIODevice::WriteOnly);
    pixmap.save(&pixmapBuffer, "BMP");

    CommString response(msgId);
    response.append(responseToFinderArgSeparator);
    response.append(CommonUtility::qStr2CommString(QString(pixmapBuffer.data().toBase64())));
    channel->sendMessage(response);
}
#endif

#if defined(KD_MACOS)
void ExtensionJob::commandRetrieveFolderStatus(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel) {
    // This command is the same as RETRIEVE_FILE_STATUS
    commandRetrieveFileStatus(argument, channel);
}

void ExtensionJob::commandMakeOnlineOnlyDirect(const CommString &argument, std::shared_ptr<AbstractCommChannel>) {
    const auto fileList = CommonUtility::splitCommString(argument, messageArgSeparator);

    {
        const std::scoped_lock lock(_dehydrationMutex);
        _nbOfOngoingDehydration++;
    }

    std::vector<SyncPath> fileListExpanded;
    processFileList(fileList, fileListExpanded);

    for (const auto &filePath: qAsConst(fileListExpanded)) {
        if (_dehydrationCanceled) {
            break;
        }

        const auto fileData = FileData::get(filePath);
        if (!fileData.syncDbId) {
            LOGW_WARN(Log::instance()->getLogger(), L"No file data - " << Utility::formatSyncPath(filePath));
            continue;
        }

        if (fileData.isLink) {
            LOGW_DEBUG(Log::instance()->getLogger(), L"Don't dehydrate symlinks - " << Utility::formatSyncPath(filePath));
            continue;
        }

        // Get pin state
        PinState ps{PinState::Unknown};
        if (const auto exitInfo = getPinState(fileData, ps); !exitInfo) {
            LOGW_INFO(Log::instance()->getLogger(),
                      L"Error in ExtensionJob::pinState - " << Utility::formatSyncPath(filePath) << L": " << exitInfo);
            continue;
        }

        // Set pin state
        if (const auto exitInfo = setPinState(fileData, PinState::OnlineOnly); !exitInfo) {
            LOGW_INFO(Log::instance()->getLogger(),
                      L"Error in ExtensionJob::setPinState - " << Utility::formatSyncPath(filePath) << L": " << exitInfo);
            continue;
        }

        // Dehydrate placeholder
        if (const auto exitInfo = dehydratePlaceholder(fileData); !exitInfo) {
            LOGW_INFO(Log::instance()->getLogger(), L"Error in ExtensionJob::dehydratePlaceholder - "
                                                            << Utility::formatSyncPath(filePath) << L": " << exitInfo);

            // Setting back the previous pin state
            if (const auto exitInfo = cancelDehydrate(fileData, ps); !exitInfo) {
                LOGW_INFO(Log::instance()->getLogger(),
                          L"Error in ExtensionJob::cancelDehydrate - " << Utility::formatSyncPath(filePath) << L": " << exitInfo);
            }

            continue;
        }
    }

    {
        const std::scoped_lock lock(_dehydrationMutex);
        _nbOfOngoingDehydration--;
        if (_nbOfOngoingDehydration == 0) {
            _dehydrationCanceled = false;
        }
    }
}

void ExtensionJob::commandCancelDehydrationDirect(const CommString &, std::shared_ptr<AbstractCommChannel>) {
    LOG_INFO(Log::instance()->getLogger(), "Ongoing files dehydrations canceled");
    _dehydrationCanceled = true;
    return;
}

void ExtensionJob::commandCancelHydrationDirect(const CommString &argument, std::shared_ptr<AbstractCommChannel>) {
    LOG_INFO(Log::instance()->getLogger(), "Ongoing files hydrations canceled");

    const auto fileList = CommonUtility::splitCommString(argument, messageArgSeparator);
    if (fileList.size() == 0) {
        return;
    }

    Sync sync;
    if (!syncForPath(fileList[0], sync)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Sync not found - " << Utility::formatSyncPath(fileList[0]));
        return;
    }

    if (!cancelDownloadJobs(sync.dbId(), fileList)) {
        LOGW_INFO(Log::instance()->getLogger(), L"Error in ExtensionJob::cancelDownloadJobs");
        return;
    }
}

void ExtensionJob::commandSetThumbnail(const CommString &argument, std::shared_ptr<AbstractCommChannel>) {
    if (argument.empty()) {
        // No thumbnail for root
        return;
    }

    if (!QFileInfo(SyncName2QStr(argument)).isFile()) {
        LOGW_WARN(Log::instance()->getLogger(), L"Not a file - " << Utility::formatSyncPath(argument));
        return;
    }

    FileData fileData = FileData::get(argument);
    if (!fileData.syncDbId) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"The file is not in a synchronized folder - " << Utility::formatSyncPath(argument));
        return;
    }

    // Find SyncPal and Vfs associated to sync
    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex, _commManager->appServer().vfsMapMutex);
    const auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _commManager->appServer().syncPalMap.end() || !syncPalMapIt->second) return;

    const auto vfsMapIt = retrieveVfsMapIt(fileData.syncDbId);
    if (vfsMapIt == _commManager->appServer().vfsMap.end() || !vfsMapIt->second) return;

    // Get NodeId
    NodeId nodeId;
    ExitCode exitCode = syncPalMapIt->second->fileRemoteIdFromLocalPath(fileData.relativePath, nodeId);
    if (exitCode != ExitCode::Ok) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in SyncPal::itemId - " << Utility::formatSyncPath(argument));
        return;
    }

    // Get thumbnail
    std::string thumbnail;
    exitCode = _commManager->appServer().getThumbnail(fileData.driveDbId, nodeId, 256, thumbnail);
    if (exitCode != ExitCode::Ok) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in getThumbnail - " << Utility::formatSyncPath(argument));
        return;
    }

    QByteArray thumbnailArr(thumbnail.c_str(), static_cast<qsizetype>(thumbnail.length()));
    QPixmap pixmap;
    if (!pixmap.loadFromData(thumbnailArr)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in QPixmap::loadFromData - " << Utility::formatSyncPath(argument));
        return;
    }

    LOG_DEBUG(Log::instance()->getLogger(), "Thumbnail fetched - size=" << pixmap.width() << "x" << pixmap.height());

    // Set thumbnail
    if (!vfsMapIt->second->setThumbnail(fileData.localPath, pixmap)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in setThumbnail - " << Utility::formatSyncPath(argument));
        return;
    }
}
#endif

void ExtensionJob::executeCommand(const CommString &commandLineStr, std::shared_ptr<AbstractCommChannel> channel) {
    const auto commands = CommonUtility::splitCommString(commandLineStr, messageCdeSeparator);
    std::string command = CommonUtility::commString2Str(commands[0]);

    CommString argument(commandLineStr);
    argument.erase(0, commands[0].length() + 1);

    if (_commands.find(command) == _commands.end()) {
        LOGW_WARN(Log::instance()->getLogger(), L"The command is not supported by this version of the client - cmd="
                                                        << CommonUtility::s2ws(command) << L" arg="
                                                        << CommonUtility::commString2WStr(argument));
        return;
    }

    _commands[command](argument, channel);
}

void ExtensionJob::manageActionsOnSingleFile(std::shared_ptr<AbstractCommChannel> channel, const SyncPath &path,
                                             SyncPalMap::const_iterator syncPalMapIt, VfsMap::const_iterator vfsMapIt,
                                             const Sync &sync) {
    bool exists = false;
    auto ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(path, exists, ioError, IoHelper::PathCheckOption::Insensitive) || !exists) {
        return;
    }

    FileData fileData = FileData::get(path);
    if (fileData.localPath.empty()) {
        return;
    }
    bool isExcluded = vfsMapIt->second->isExcluded(fileData.localPath);
    if (isExcluded) {
        return;
    }

    NodeId nodeId;
    ExitCode exitCode = syncPalMapIt->second->fileRemoteIdFromLocalPath(fileData.relativePath, nodeId);
    if (exitCode != ExitCode::Ok) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in SyncPal::itemId - " << Utility::formatSyncPath(fileData.relativePath));
        return;
    }
    bool isOnTheServer = !nodeId.empty();

    if (sync.dbId()) {
        CommString response(Str("VFS_MODE"));
        response.append(responseToFinderArgSeparator);
        response.append(Vfs::modeToString(sync.virtualFileMode()));
        channel->sendMessage(response);
    }

    if (sync.dbId()) {
        sendSharingContextMenuOptions(fileData, channel);
        buildAndSendMenuItemMessage(channel, Str("OPEN_PRIVATE_LINK"), isOnTheServer, openInBrowserText());
    }
}

void ExtensionJob::fetchPrivateLinkUrlHelper(const SyncPath &localFile,
                                             const std::function<void(const std::string &url)> &targetFun) {
    // Find the common sync
    Sync sync;
    if (!syncForPath(localFile, sync)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Sync not found - " << Utility::formatSyncPath(localFile));
        return;
    }

    Drive drive;
    bool found = false;
    if (!ParmsDb::instance()->selectDrive(sync.driveDbId(), drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        return;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive not found - id=" << sync.driveDbId());
        return;
    }

    // Find the syncpal associated to sync
    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    const auto syncPalMapIt = retrieveSyncPalMapIt(sync.dbId());
    if (syncPalMapIt == _commManager->appServer().syncPalMap.end() || !syncPalMapIt->second) return;

    const FileData fileData = FileData::get(localFile);
    if (!fileData.isValid()) return;

    NodeId itemId;
    if (syncPalMapIt->second->fileRemoteIdFromLocalPath(fileData.relativePath, itemId) != ExitCode::Ok) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in SyncPal::itemId - " << Utility::formatSyncPath(localFile));
        return;
    }

    const QString linkUrl = QString(APPLICATION_PREVIEW_URL).arg(drive.driveId()).arg(itemId.c_str());
    targetFun(linkUrl.toStdString());
}

bool ExtensionJob::syncFileStatus(const FileData &fileData, SyncFileStatus &status, VfsStatus &vfsStatus) {
    status = SyncFileStatus::Unknown;
    vfsStatus.isPlaceholder = false;
    vfsStatus.isHydrated = false;

    if (!fileData.isValid()) return false;

    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex, _commManager->appServer().vfsMapMutex);

    const auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _commManager->appServer().syncPalMap.end() || !syncPalMapIt->second) return false;

    bool exists = false;
    if (!syncPalMapIt->second->checkIfExistsOnServer(fileData.relativePath, exists)) {
        LOGW_DEBUG(Log::instance()->getLogger(),
                   L"Error in SyncPal::checkIfExistsOnServer: " << Utility::formatSyncPath(fileData.relativePath));
        // Occurs when the sync is stopped
        return false;
    }

    if (exists) {
        status = SyncFileStatus::Success;
    }

    const auto vfsMapIt = retrieveVfsMapIt(fileData.syncDbId);
    if (vfsMapIt == _commManager->appServer().vfsMap.end() || !vfsMapIt->second) return false;

    if (vfsMapIt->second->mode() == VirtualFileMode::Mac || vfsMapIt->second->mode() == VirtualFileMode::Win) {
        if (!vfsMapIt->second->status(fileData.localPath, vfsStatus)) {
            LOGW_WARN(Log::instance()->getLogger(), L"Error in Vfs::status - " << Utility::formatSyncPath(fileData.localPath));
            return false;
        }

        if (vfsStatus.isSyncing) {
            status = SyncFileStatus::Syncing;
        }
    }

    return true;
}

SyncPalMap::const_iterator ExtensionJob::retrieveSyncPalMapIt(const SyncDbId syncDbId) const {
    const auto result = _commManager->appServer().syncPalMap.find(syncDbId);
    if (result == _commManager->appServer().syncPalMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "SyncPal not found in SyncPalMap - syncDbId=" << syncDbId);
        return _commManager->appServer().syncPalMap.end();
    }

    return result;
}

VfsMap::const_iterator ExtensionJob::retrieveVfsMapIt(const SyncDbId syncDbId) const {
    const auto result = _commManager->appServer().vfsMap.find(syncDbId);
    if (result == _commManager->appServer().vfsMap.cend()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in VfsMap - syncDbId=" << syncDbId);
        return _commManager->appServer().vfsMap.cend();
    }

    return result;
}

ExitInfo ExtensionJob::getPinState(const FileData &fileData, PinState &pinState) {
    if (!fileData.syncDbId) return {ExitCode::LogicError, ExitCause::InvalidArgument};

    const std::scoped_lock lock(_commManager->appServer().vfsMapMutex);
    const auto vfsMapIt = retrieveVfsMapIt(fileData.syncDbId);
    if (vfsMapIt == _commManager->appServer().vfsMap.cend() || !vfsMapIt->second) return {ExitCode::LogicError};

    pinState = vfsMapIt->second->pinState(fileData.relativePath);
    return ExitCode::Ok;
}

ExitInfo ExtensionJob::setPinState(const FileData &fileData, PinState pinState) {
    if (!fileData.syncDbId) return {ExitCode::LogicError, ExitCause::InvalidArgument};

    const std::scoped_lock lock(_commManager->appServer().vfsMapMutex);
    const auto vfsMapIt = retrieveVfsMapIt(fileData.syncDbId);
    if (vfsMapIt == _commManager->appServer().vfsMap.cend() || !vfsMapIt->second) return {ExitCode::LogicError};

    return vfsMapIt->second->setPinState(fileData.relativePath, pinState);
}

ExitInfo ExtensionJob::cancelHydrate(const FileData &fileData, PinState pinState) {
    if (!fileData.syncDbId) return {ExitCode::LogicError, ExitCause::InvalidArgument};

    const std::scoped_lock lock(_commManager->appServer().vfsMapMutex);
    const auto vfsMapIt = retrieveVfsMapIt(fileData.syncDbId);
    if (vfsMapIt == _commManager->appServer().vfsMap.cend() || !vfsMapIt->second) return {ExitCode::LogicError};

    vfsMapIt->second->cancelHydrate(fileData.localPath);
    return vfsMapIt->second->setPinState(fileData.relativePath, pinState);
}

ExitInfo ExtensionJob::cancelDehydrate(const FileData &fileData, PinState pinState) {
    return setPinState(fileData, pinState);
}

ExitInfo ExtensionJob::dehydratePlaceholder(const FileData &fileData) {
    if (!fileData.syncDbId) return {ExitCode::LogicError, ExitCause::InvalidArgument};

    const std::scoped_lock lock(_commManager->appServer().vfsMapMutex);
    const auto vfsMapIt = retrieveVfsMapIt(fileData.syncDbId);
    if (vfsMapIt == _commManager->appServer().vfsMap.cend() || !vfsMapIt->second) return {ExitCode::LogicError};

    return vfsMapIt->second->dehydratePlaceholder(fileData.relativePath);
}

bool ExtensionJob::addDownloadJob(const FileData &fileData, const SyncPath &parentFolderPath) {
    if (!fileData.syncDbId) return false;
    const std::scoped_lock lock(_commManager->appServer().vfsMapMutex);
    const auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _commManager->appServer().syncPalMap.end() || !syncPalMapIt->second) return false;

    // Create download job
    const ExitCode exitCode = syncPalMapIt->second->addDlDirectJob(fileData.relativePath, fileData.localPath, parentFolderPath);
    if (exitCode != ExitCode::Ok) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in SyncPal::addDownloadJob - " << Utility::formatSyncPath(fileData.relativePath));
        return false;
    }

    return true;
}

#if defined(KD_MACOS)
bool ExtensionJob::cancelDownloadJobs(const SyncDbId syncDbId, const std::vector<CommString> &fileList) {
    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);

    const auto syncPalMapIt = retrieveSyncPalMapIt(syncDbId);
    if (syncPalMapIt == _commManager->appServer().syncPalMap.end() || !syncPalMapIt->second) return false;

    std::vector<SyncPath> syncPathList;
    processFileList(fileList, syncPathList);

    // Cancel download jobs
    const ExitCode exitCode = syncPalMapIt->second->cancelDlDirectJobs(syncPathList);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncPal::cancelDlDirectJobs");
        return false;
    }

    return true;
}
#endif

void ExtensionJob::copyUrlToClipboard(const std::string &link) {
#if defined(KD_WINDOWS)
    const size_t len = link.size() + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    (void) memcpy(GlobalLock(hMem), link.c_str(), len);
    (void) GlobalUnlock(hMem);
    if (!OpenClipboard(nullptr)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in OpenClipboard: err=" << GetLastError());
        return;
    }
    (void) EmptyClipboard();
    if (!SetClipboardData(CF_TEXT, hMem)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SetClipboardData: err=" << GetLastError());
        return;
    }
    (void) CloseClipboard();
#else
    QApplication::clipboard()->setText(QString::fromStdString(link));
#endif
}

std::string ExtensionJob::statusString(SyncFileStatus status, const VfsStatus &vfsStatus) const {
    std::string statusString;

    switch (status) {
        case SyncFileStatus::Unknown:
            statusString = "NOP";
            break;
        case SyncFileStatus::Syncing:
            statusString = "SYNC_";
            statusString.append(std::to_string(vfsStatus.progress));
            break;
        case SyncFileStatus::Conflict:
        case SyncFileStatus::Ignored:
            statusString = "IGNORE";
            break;
        case SyncFileStatus::Success:
        case SyncFileStatus::Inconsistency:
            if ((vfsStatus.isPlaceholder && vfsStatus.isHydrated) || !vfsStatus.isPlaceholder) {
                statusString = "OK";
            } else {
                statusString = "ONLINE";
            }
            break;
        case SyncFileStatus::Error:
            statusString = "ERROR";
            break;
        case SyncFileStatus::EnumEnd:
            assert(false && "Invalid enum value in switch statement.");
    }

    return statusString;
}

void ExtensionJob::openPrivateLink(const std::string &link) {
    openBrowser(link);
}

void ExtensionJob::sendSharingContextMenuOptions(const FileData &fileData, std::shared_ptr<AbstractCommChannel> channel) {
    auto theme = Theme::instance();
    if (!(theme->userGroupSharing() || theme->linkSharing())) return;
    if (!fileData.syncDbId) return;

    // Find SyncPal associated to sync
    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    const auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _commManager->appServer().syncPalMap.end() || !syncPalMapIt->second) return;

    bool isOnTheServer = false;
    if (!syncPalMapIt->second->checkIfExistsOnServer(fileData.relativePath, isOnTheServer)) {
        LOGW_DEBUG(Log::instance()->getLogger(),
                   L"Error in SyncPal::checkIfExistsOnServer: " << Utility::formatSyncPath(fileData.relativePath));
        // Occurs when the sync is stopped
        return;
    }

    bool canShare = false;
    if (!syncPalMapIt->second->checkIfCanShareItem(fileData.relativePath, canShare)) {
        LOGW_DEBUG(Log::instance()->getLogger(),
                   L"Error in SyncPal::checkIfCanShareItem: " << Utility::formatSyncPath(fileData.relativePath));
        // Occurs when the sync is stopped
        return;
    }

    // If sharing is globally disabled, do not show any sharing entries.
    // If there is no permission to share for this file, add a disabled entry saying so
    if (isOnTheServer && !canShare) {
        buildAndSendMenuItemMessage(channel, Str("DISABLED"), false, resharingText(fileData.isDirectory));
    } else {
        // Do we have public links?
        bool publicLinksEnabled = theme->linkSharing();

        // Is is possible to create a public link without user choices?
        bool canCreateDefaultPublicLink = publicLinksEnabled;

        if (canCreateDefaultPublicLink) {
            buildAndSendMenuItemMessage(channel, Str("COPY_PUBLIC_LINK"), isOnTheServer, copyPublicShareLinkText());
        } else if (publicLinksEnabled) {
            buildAndSendMenuItemMessage(channel, Str("MANAGE_PUBLIC_LINKS"), isOnTheServer, copyPublicShareLinkText());
        }
    }

    buildAndSendMenuItemMessage(channel, Str("COPY_PRIVATE_LINK"), isOnTheServer, copyPrivateShareLinkText());
}

void ExtensionJob::addSharingContextMenuOptions(const FileData &fileData, CommString &response) {
    auto theme = Theme::instance();
    if (!(theme->userGroupSharing() || theme->linkSharing())) return;
    if (!fileData.syncDbId) return;

    // Find SyncPal associated to sync
    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    const auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _commManager->appServer().syncPalMap.end() || !syncPalMapIt->second) return;

    bool isOnTheServer = false;
    if (!syncPalMapIt->second->checkIfExistsOnServer(fileData.relativePath, isOnTheServer)) {
        LOGW_DEBUG(Log::instance()->getLogger(),
                   L"Error in SyncPal::checkIfExistsOnServer: " << Utility::formatSyncPath(fileData.relativePath));
        // Occurs when the sync is stopped
        return;
    }

    bool canShare = false;
    if (!syncPalMapIt->second->checkIfCanShareItem(fileData.relativePath, canShare)) {
        LOGW_DEBUG(Log::instance()->getLogger(),
                   L"Error in SyncPal::checkIfCanShareItem: " << Utility::formatSyncPath(fileData.relativePath));
        // Occurs when the sync is stopped
        return;
    }


    // If sharing is globally disabled, do not show any sharing entries.
    // If there is no permission to share for this file, add a disabled entry saying so
    if (isOnTheServer && !canShare) {
        response.append(responseToFinderArgSeparator);
        response.append(Str("DISABLED"));
        response.append(responseToFinderArgSeparator);
        response.append(Str("d"));
        response.append(responseToFinderArgSeparator);
        response.append(resharingText(fileData.isDirectory));
    } else {
        // Do we have public links?
        bool publicLinksEnabled = theme->linkSharing();

        if (publicLinksEnabled) {
            response.append(responseToFinderArgSeparator);
            response.append(Str("COPY_PUBLIC_LINK"));
            response.append(responseToFinderArgSeparator);
            response.append(isOnTheServer ? Str("") : Str("d"));
            response.append(responseToFinderArgSeparator);
            response.append(copyPublicShareLinkText());
        }
    }

    response.append(responseToFinderArgSeparator);
    response.append(Str("COPY_PRIVATE_LINK"));
    response.append(responseToFinderArgSeparator);
    response.append(isOnTheServer ? Str("") : Str("d"));
    response.append(responseToFinderArgSeparator);
    response.append(copyPrivateShareLinkText());
}


void ExtensionJob::buildAndSendMenuItemMessage(std::shared_ptr<AbstractCommChannel> channel, const CommString &type, bool enabled,
                                               const CommString &text) {
    CommString response(Str("MENU_ITEM"));
    response.append(responseToFinderArgSeparator);
    response.append(type);
    response.append(responseToFinderArgSeparator);
    response.append(enabled ? Str("") : Str("d"));
    response.append(responseToFinderArgSeparator);
    response.append(text);
    channel->sendMessage(response);
}

#if defined(KD_MACOS)
void ExtensionJob::processFileList(const std::vector<CommString> &inFileList, std::vector<SyncPath> &outFileList) {
    // Process all files
    for (const auto &path: inFileList) {
        const FileData fileData = FileData::get(path);
        if (!fileData.isValid() || fileData.isLink) continue;

        if (fileData.isDirectory) {
            auto ioError = IoError::Success;
            IoHelper::DirectoryIterator dirIt;
            bool endOfDir = false;
            DirectoryEntry entry;

            try {
                if (!IoHelper::getRecursiveDirectoryIterator(path, ioError, dirIt, true)) {
                    LOGW_WARN(_logger, L"Error in IoHelper::recursiveDirectoryIterator");
                    continue;
                }

                while (dirIt.next(entry, endOfDir, ioError) && !endOfDir) {
                    FileData tmpFileData = FileData::get(entry.path());
                    if (!tmpFileData.isValid() || tmpFileData.isLink || tmpFileData.isDirectory) continue;

                    auto status = SyncFileStatus::Unknown;
                    if (VfsStatus vfsStatus; !syncFileStatus(tmpFileData, status, vfsStatus)) {
                        LOGW_WARN(Log::instance()->getLogger(),
                                  L"Error in ExtensionJob::syncFileStatus: " << Utility::formatSyncPath(entry.path()));
                        continue;
                    }

                    if (status == SyncFileStatus::Unknown || status == SyncFileStatus::Ignored) {
                        continue;
                    }

                    outFileList.push_back(entry.path());
                }
            } catch (std::filesystem::filesystem_error &e) {
                LOG_WARN(Log::instance()->getLogger(),
                         "Error caught in ExtensionJob::processFileList: code=" << e.code() << " error=" << e.what());
            } catch (...) {
                LOG_WARN(Log::instance()->getLogger(), "Error caught in ExtensionJob::processFileList");
            }
        } else {
            outFileList.push_back(path);
        }
    }
}
#endif

CommString ExtensionJob::vfsPinActionText() {
    return CommonUtility::qStr2CommString(QObject::tr("Make available locally"));
}

CommString ExtensionJob::vfsFreeSpaceActionText() {
    return CommonUtility::qStr2CommString(QObject::tr("Free up local space"));
}

CommString ExtensionJob::cancelDehydrationText() {
    return CommonUtility::qStr2CommString(QObject::tr("Cancel free up local space"));
}

CommString ExtensionJob::cancelHydrationText() {
    return CommonUtility::qStr2CommString(QObject::tr("Cancel make available locally"));
}

CommString ExtensionJob::resharingText(bool isDirectory) {
    return isDirectory ? CommonUtility::qStr2CommString(QObject::tr("Resharing this file is not allowed"))
                       : CommonUtility::qStr2CommString(QObject::tr("Resharing this folder is not allowed"));
}

CommString ExtensionJob::copyPublicShareLinkText() {
    return CommonUtility::qStr2CommString(QObject::tr("Copy public share link"));
}

CommString ExtensionJob::copyPrivateShareLinkText() {
    return CommonUtility::qStr2CommString(QObject::tr("Copy private share link"));
}

CommString ExtensionJob::openInBrowserText() {
    return CommonUtility::qStr2CommString(QObject::tr("Open in browser"));
}

bool ExtensionJob::openBrowser(const std::string &url) {
    if (!QDesktopServices::openUrl(QString::fromStdString(url))) {
        LOG_WARN(Log::instance()->getLogger(), "QDesktopServices::openUrl failed - url=" << url);
        return false;
    }
    return true;
}

CommString ExtensionJob::buildMessage(const std::string &verb, const SyncPath &path, const std::string &status) {
    CommString msg(CommonUtility::str2CommString(verb));

    if (!status.empty()) {
        msg.append(responseToFinderArgSeparator);
        msg.append(CommonUtility::str2CommString(status));
    }
    if (!path.empty()) {
        msg.append(responseToFinderArgSeparator);
        msg.append(path.native());
    }
    return msg;
}

void ExtensionJob::monitorFolderHydration(const FileData &fileData) const {
    if (!fileData.syncDbId) return;

    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    const auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _commManager->appServer().syncPalMap.end() || !syncPalMapIt->second) return;

    syncPalMapIt->second->monitorFolderHydration(fileData.localPath);
}

FileData FileData::get(const SyncPath &path) {
#if defined(KD_WINDOWS)
    SyncPath tmpPath;
    bool notFound = false;
    if (!Utility::longPath(path, tmpPath, notFound)) {
        if (notFound) {
            LOGW_WARN(Log::instance()->getLogger(), L"File not found - " << Utility::formatSyncPath(path));
        } else {
            LOGW_WARN(Log::instance()->getLogger(), L"Error in Utility::longpath - " << Utility::formatSyncPath(path));
        }
        return {};
    }
#else
    SyncPath tmpPath(path);
#endif

    Sync sync;
    if (!syncForPath(tmpPath, sync)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Sync not found - " << Utility::formatSyncPath(tmpPath));
        return {};
    }

    FileData data;
    data.syncDbId = sync.dbId();
    data.driveDbId = sync.driveDbId();
    data.virtualFileMode = sync.virtualFileMode();
    data.localPath = tmpPath.native();
    data.relativePath = CommonUtility::relativePath(sync.localPath(), tmpPath).native();

    ItemType itemType;
    if (!IoHelper::getItemType(tmpPath, itemType)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in Utility::getItemType: " << Utility::formatIoError(tmpPath, itemType.ioError));
        return {};
    }

    if (itemType.ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_DEBUG(Log::instance()->getLogger(), L"Item does not exist anymore - " << Utility::formatSyncPath(tmpPath));
        return {};
    }

    data.isLink = itemType.linkType != LinkType::None;
    if (data.isLink) {
        data.isDirectory = false;
    } else {
        std::error_code ec;
        data.isDirectory = std::filesystem::is_directory(tmpPath, ec);
        if (!data.isDirectory && ec.value() != 0) {
            if (const bool exists =
                        !utility_base::isLikeFileNotFoundError(ec) || utility_base::isLikeTooManySymbolicLinkLevelsError(ec);
                !exists) {
                // Item does not exist anymore.
                LOGW_DEBUG(Log::instance()->getLogger(), L"Item does not exist - " << Utility::formatSyncPath(data.localPath));
            } else {
                LOGW_WARN(Log::instance()->getLogger(), L"Failed to check if the path is a directory - "
                                                                << Utility::formatSyncPath(data.localPath) << L", "
                                                                << Utility::formatStdError(ec));
            }
            return {};
        }
    }

    return data;
}

FileData FileData::parentFolder() const {
    return FileData::get(localPath.parent_path());
}

} // namespace KDC
