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

bool syncForPath(const std::filesystem::path &path, Sync &sync) {
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        return false;
    }

    for (const Sync &tmpSync: syncList) {
        if (CommonUtility::isSubDir(tmpSync.localPath(), path)) {
            sync = tmpSync;
            return true;
        }
    }

    return false;
}

bool ExtensionJob::_dehydrationCanceled = false;
unsigned ExtensionJob::_nbOngoingDehydration = 0;
std::mutex ExtensionJob::_dehydrationMutex;

ExtensionJob::ExtensionJob(std::shared_ptr<CommManager> commManager, const CommString &commandLineStr,
                           const std::list<std::shared_ptr<AbstractCommChannel>> &channels) :
    _commManager(commManager),
    _commandLineStr(commandLineStr),
    _channels(channels) {
    _commands =
    { {"REGISTER_PATH", std::bind(&ExtensionJob::commandRegisterFolder, this, std::placeholders::_1, std::placeholders::_2)},
      {"UNREGISTER_PATH", std::bind(&ExtensionJob::commandUnregisterFolder, this, std::placeholders::_1, std::placeholders::_2)},
      {"GET_STRINGS", std::bind(&ExtensionJob::commandGetStrings, this, std::placeholders::_1, std::placeholders::_2)},
      {"STATUS", std::bind(&ExtensionJob::commandForceStatus, this, std::placeholders::_1, std::placeholders::_2)},
      {"GET_MENU_ITEMS", std::bind(&ExtensionJob::commandGetMenuItems, this, std::placeholders::_1, std::placeholders::_2)},
      {"COPY_PUBLIC_LINK", std::bind(&ExtensionJob::commandCopyPublicLink, this, std::placeholders::_1, std::placeholders::_2)},
      {"COPY_PRIVATE_LINK", std::bind(&ExtensionJob::commandCopyPrivateLink, this, std::placeholders::_1, std::placeholders::_2)},
      {"OPEN_PRIVATE_LINK", std::bind(&ExtensionJob::commandOpenPrivateLink, this, std::placeholders::_1, std::placeholders::_2)},
      {"MAKE_AVAILABLE_LOCALLY_DIRECT",
       std::bind(&ExtensionJob::commandMakeAvailableLocallyDirect, this, std::placeholders::_1, std::placeholders::_2)},
      {"RETRIEVE_FILE_STATUS",
       std::bind(&ExtensionJob::commandRetrieveFileStatus, this, std::placeholders::_1, std::placeholders::_2)},
#if defined(KD_WINDOWS)
      {"GET_ALL_MENU_ITEMS",
       std::bind(&ExtensionJob::commandGetAllMenuItems, this, std::placeholders::_1, std::placeholders::_2)},
      {"GET_THUMBNAIL", std::bind(&ExtensionJob::commandGetThumbnail, this, std::placeholders::_1, std::placeholders::_2)},
#endif
#if defined(KD_MACOS)
      {"RETRIEVE_FOLDER_STATUS",
       std::bind(&ExtensionJob::commandRetrieveFolderStatus, this, std::placeholders::_1, std::placeholders::_2)},
      {"MAKE_ONLINE_ONLY_DIRECT",
       std::bind(&ExtensionJob::commandMakeOnlineOnlyDirect, this, std::placeholders::_1, std::placeholders::_2)},
      {"CANCEL_DEHYDRATION_DIRECT",
       std::bind(&ExtensionJob::commandCancelDehydrationDirect, this, std::placeholders::_1, std::placeholders::_2)},
      {"CANCEL_HYDRATION_DIRECT",
       std::bind(&ExtensionJob::commandCancelHydrationDirect, this, std::placeholders::_1, std::placeholders::_2)},
      {"SET_THUMBNAIL", std::bind(&ExtensionJob::commandSetThumbnail, this, std::placeholders::_1, std::placeholders::_2)}
#endif
    };
}

void ExtensionJob::runJob() {
    if (_channels.empty()) {
        executeCommand(_commandLineStr, nullptr);
    } else {
        foreach (auto &channel, _channels) {
            executeCommand(_commandLineStr, channel);
        }
    }

    _exitInfo = ExitCode::Ok;
}

void ExtensionJob::commandGetMenuItems(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel) {
    {
        CommString response(Str("GET_MENU_ITEMS"));
        response.append(responseToFinderArgSeparator);
        response.append(Str("BEGIN"));
        channel->sendMessage(response);
    }

    const auto files = CommonUtility::splitCommString(argument, messageArgSeparator);

    // Find the common sync
    Sync sync;
    for (const auto &file: files) {
        Sync tmpSync;
        if (!syncForPath(file, tmpSync)) {
            return;
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

    // Find SyncPal and Vfs associated to sync
    std::unordered_map<int, std::shared_ptr<SyncPal>>::const_iterator syncPalMapIt = _commManager->_syncPalMap.end();
    std::unordered_map<int, std::shared_ptr<Vfs>>::const_iterator vfsMapIt = _commManager->_vfsMap.end();
    if (sync.dbId()) {
        syncPalMapIt = retrieveSyncPalMapIt(sync.dbId());
        if (syncPalMapIt == _commManager->_syncPalMap.end()) return;

        vfsMapIt = retrieveVfsMapIt(sync.dbId());
        if (vfsMapIt == _commManager->_vfsMap.end()) return;
    }

#if defined(KD_MACOS)
    // Manage dehydration cancellation
    bool canCancelDehydration = false;

    {
        const std::lock_guard lock(_dehydrationMutex);
        if (_nbOngoingDehydration > 0) {
            canCancelDehydration = true;
        }
    }

    // File availability actions
    if (sync.dbId() && sync.virtualFileMode() != VirtualFileMode::Off && vfsMapIt->second->showPinStateActions()) {
        LOG_IF_FAIL(Log::instance()->getLogger(), !files.empty());

        // Some options only show for single files
        bool isSingleFile = false;
        if (files.size() == 1) {
            manageActionsOnSingleFile(channel, files, syncPalMapIt, vfsMapIt, sync);

            isSingleFile = QFileInfo(CommString2QStr(files[0])).isFile();
        }

        bool canHydrate = true;
        bool canDehydrate = true;
        bool canCancelHydration = false;
        for (const auto &file: files) {
            VfsStatus vfsStatus;
            if (!canCancelHydration && vfsMapIt->second->status(file, vfsStatus) && vfsStatus.isSyncing) {
                canCancelHydration = syncPalMapIt->second->isDownloadOngoing(file);
            }

            if (isSingleFile) {
                canHydrate = vfsStatus.isPlaceholder && !vfsStatus.isSyncing && !vfsStatus.isHydrated;
                canDehydrate = vfsStatus.isPlaceholder && !vfsStatus.isSyncing && vfsStatus.isHydrated;
            }
        }

        // TODO: Should be a submenu, should use icons
        auto makePinContextMenu = [&](bool makeAvailableLocally, bool freeSpace, bool cancelDehydration, bool cancelHydration) {
            buildAndSendMenuItemMessage(channel, Str("MAKE_AVAILABLE_LOCALLY_DIRECT"), makeAvailableLocally, vfsPinActionText());

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
#elif defined(KD_WINDOWS)
    // Some options only show for single files
    bool isSingleFile = false;
    if (files.size() == 1) {
        manageActionsOnSingleFile(channel, files, syncPalMapIt, vfsMapIt, sync);
    }
#endif

    {
        CommString response(Str("GET_MENU_ITEMS"));
        response.append(responseToFinderArgSeparator);
        response.append(Str("END"));
        channel->sendMessage(response);
    }
}

void ExtensionJob::commandCopyPublicLink(const CommString &argument, std::shared_ptr<AbstractCommChannel>) {
    const auto fileData = FileData::get(argument);
    if (!fileData.syncDbId) return;

    const auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _commManager->_syncPalMap.end()) return;

    // Get NodeId
    NodeId nodeId;
    ExitCode exitCode = syncPalMapIt->second->fileRemoteIdFromLocalPath(fileData.relativePath, nodeId);
    if (exitCode != ExitCode::Ok) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in SyncPal::itemId - " << Utility::formatSyncPath(fileData.relativePath));
        return;
    }

    // Get public link URL
    std::string linkUrl;
    exitCode = _commManager->_getPublicLinkUrl(fileData.driveDbId, nodeId, linkUrl);
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
        if (!fileData.syncDbId) {
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
        // Not done in Windows case: triggers a hydration
        // Set pin state
        if (!setPinState(fileData, PinState::AlwaysLocal)) {
            LOGW_INFO(Log::instance()->getLogger(),
                      L"Error in ExtensionJob::setPinState - " << Utility::formatSyncPath(filePath));
            continue;
        }
#endif

        if (!addDownloadJob(fileData, parentFolder)) {
            LOGW_INFO(Log::instance()->getLogger(),
                      L"Error in ExtensionJob::addDownloadJob - " << Utility::formatSyncPath(filePath));
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
        LOGW_WARN(Log::instance()->getLogger(), L"Invalid argument - arg=" << CommString2WStr(argument));
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
            {{Str("CONTEXT_MENU_TITLE"), Str2CommString(Theme::instance()->appName())}}};

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
    auto argumentList = CommonUtility::splitCommString(argument, messageArgSeparator);

    CommString msgId = argumentList[0];
    argumentList.erase(argumentList.begin());

    CommString response(msgId);
    response.append(responseToFinderArgSeparator);
    response.append(Str2CommString(Theme::instance()->appName()));

    // Find the common sync
    Sync sync;
    for (const auto &filePathStr: argumentList) {
        Sync tmpSync;
        if (!syncForPath(filePathStr, tmpSync)) {
            channel->sendMessage(response);
            return;
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

    std::unordered_map<int, std::shared_ptr<SyncPal>>::const_iterator syncPalMapIt;
    std::unordered_map<int, std::shared_ptr<Vfs>>::const_iterator vfsMapIt;
    if (sync.dbId()) {
        syncPalMapIt = retrieveSyncPalMapIt(sync.dbId());
        if (syncPalMapIt == _commManager->_syncPalMap.end()) {
            channel->sendMessage(response);
            return;
        }

        vfsMapIt = retrieveVfsMapIt(sync.dbId());
        if (vfsMapIt == _commManager->_vfsMap.end()) {
            channel->sendMessage(response);
            return;
        }
    }

    response.append(responseToFinderArgSeparator);
    response.append(sync.dbId() ? Vfs::modeToString(vfsMapIt->second->mode()) : Str(""));

    // Some options only show for single files
    if (argumentList.size() == 1) {
        FileData fileData = FileData::get(argumentList[0]);
        NodeId nodeId;
        ExitCode exitCode = syncPalMapIt->second->fileRemoteIdFromLocalPath(fileData.relativePath, nodeId);
        if (exitCode != ExitCode::Ok) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in SyncPal::itemId - " << Utility::formatSyncPath(fileData.relativePath));
            channel->sendMessage(response);
            return;
        }
        bool isOnTheServer = !nodeId.empty();

        if (sync.dbId()) {
            addSharingContextMenuOptions(fileData, response);
            response.append(responseToFinderArgSeparator);
            response.append(Str("OPEN_PRIVATE_LINK"));
            response.append(responseToFinderArgSeparator);
            response.append(isOnTheServer ? Str("") : Str("d"));
            response.append(responseToFinderArgSeparator);
            response.append(openInBrowserText());
        }
    }

    bool canCancelHydration = false;
    bool canCancelDehydration = false;

    // File availability actions
    if (sync.dbId() && sync.virtualFileMode() != VirtualFileMode::Off) {
        LOG_IF_FAIL(Log::instance()->getLogger(), !argumentList.empty());

        for (const auto &filePathStr: argumentList) {
            SyncPath filePath(filePathStr);
            auto fileData = FileData::get(filePath);
            if (syncPalMapIt->second->isDownloadOngoing(fileData.relativePath)) {
                canCancelHydration = true;
                break;
            }
        }
    }

    if (canCancelDehydration) {
        response.append(responseToFinderArgSeparator);
        response.append(Str("CANCEL_DEHYDRATION_DIRECT"));
        response.append(responseToFinderArgSeparator);
        response.append(responseToFinderArgSeparator);
        response.append(cancelDehydrationText());
    }

    if (canCancelHydration) {
        response.append(responseToFinderArgSeparator);
        response.append(Str("CANCEL_HYDRATION_DIRECT"));
        response.append(responseToFinderArgSeparator);
        response.append(responseToFinderArgSeparator);
        response.append(cancelHydrationText());
    }

    channel->sendMessage(response);
}

void ExtensionJob::commandGetThumbnail(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel) {
    const auto argumentList = CommonUtility::splitCommString(argument, messageArgSeparator);

    if (argumentList.size() != 3) {
        LOGW_WARN(Log::instance()->getLogger(), L"Invalid argument - arg=" << CommString2WStr(argument));
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
    if (!QFileInfo(CommString2QStr(filePath)).isFile()) {
        LOGW_WARN(Log::instance()->getLogger(), L"Not a file - " << Utility::formatSyncPath(filePath));
        return;
    }

    FileData fileData = FileData::get(filePath);
    if (!fileData.syncDbId) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"The file is not in a synchonized folder - " << Utility::formatSyncPath(filePath));
        return;
    }

    auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _commManager->_syncPalMap.end()) return;

    // Get NodeId
    NodeId nodeId;
    ExitCode exitCode = syncPalMapIt->second->fileRemoteIdFromLocalPath(fileData.relativePath, nodeId);
    if (exitCode != ExitCode::Ok) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in SyncPal::itemId - " << Utility::formatSyncPath(filePath));
        return;
    }

    // Get thumbnail
    std::string thumbnail;
    exitCode = _commManager->_getThumbnail(fileData.driveDbId, nodeId, 256, thumbnail);
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
    response.append(QStr2CommString(QString(pixmapBuffer.data().toBase64())));
    channel->sendMessage(response);
}
#endif
#if defined(KD_MACOS)
void ExtensionJob::commandRetrieveFolderStatus(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel) {
    // This command is the same as RETRIEVE_FILE_STATUS
    commandRetrieveFileStatus(argument, channel);
}

void ExtensionJob::commandMakeOnlineOnlyDirect(const CommString &argument, std::shared_ptr<AbstractCommChannel>) {
    const auto fileList = CommonUtility::splitCommString(argument, messageCdeSeparator);

    {
        const std::lock_guard lock(_dehydrationMutex);
        _nbOngoingDehydration++;
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

        // Set pin state
        if (ExitInfo exitInfo = setPinState(fileData, PinState::OnlineOnly); !exitInfo) {
            LOGW_INFO(Log::instance()->getLogger(),
                      L"Error in ExtensionJob::setPinState - " << Utility::formatSyncPath(filePath) << L": " << exitInfo);
            continue;
        }

        // Dehydrate placeholder
        if (ExitInfo exitInfo = dehydratePlaceholder(fileData); !exitInfo) {
            LOGW_INFO(Log::instance()->getLogger(),
                      L"Error in ExtensionJob::dehydratePlaceholder - " << Utility::formatSyncPath(filePath) << exitInfo);
            continue;
        }
    }

    {
        const std::lock_guard lock(_dehydrationMutex);
        _nbOngoingDehydration--;
        if (_nbOngoingDehydration == 0) {
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
    auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _commManager->_syncPalMap.end()) return;

    auto vfsMapIt = retrieveVfsMapIt(fileData.syncDbId);
    if (vfsMapIt == _commManager->_vfsMap.end()) return;

    // Get NodeId
    NodeId nodeId;
    ExitCode exitCode = syncPalMapIt->second->fileRemoteIdFromLocalPath(fileData.relativePath, nodeId);
    if (exitCode != ExitCode::Ok) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in SyncPal::itemId - " << Utility::formatSyncPath(argument));
        return;
    }

    // Get thumbnail
    std::string thumbnail;
    exitCode = _commManager->_getThumbnail(fileData.driveDbId, nodeId, 256, thumbnail);
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
    std::string command = CommString2Str(commands[0]);

    CommString argument(commandLineStr);
    argument.erase(0, commands[0].length() + 1);

    if (_commands.find(command) == _commands.end()) {
        LOGW_WARN(Log::instance()->getLogger(), L"The command is not supported by this version of the client - cmd="
                                                        << CommonUtility::s2ws(command) << L" arg=" << CommString2WStr(argument));
        return;
    }

    _commands[command](argument, channel);
}

void ExtensionJob::manageActionsOnSingleFile(std::shared_ptr<AbstractCommChannel> channel, const std::vector<CommString> &files,
                                             std::unordered_map<int, std::shared_ptr<SyncPal>>::const_iterator syncPalMapIt,
                                             std::unordered_map<int, std::shared_ptr<Vfs>>::const_iterator vfsMapIt,
                                             const Sync &sync) {
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(files[0], exists, ioError) || !exists) {
        return;
    }

    FileData fileData = FileData::get(files[0]);
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
    std::unordered_map<int, std::shared_ptr<SyncPal>>::const_iterator syncPalMapIt = _commManager->_syncPalMap.end();
    if (sync.dbId()) {
        syncPalMapIt = retrieveSyncPalMapIt(sync.dbId());
    }

    if (syncPalMapIt == _commManager->_syncPalMap.end()) return;

    FileData fileData = FileData::get(localFile);
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

    if (!fileData.syncDbId) return false;

    const auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _commManager->_syncPalMap.end()) return false;

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
    if (vfsMapIt == _commManager->_vfsMap.end()) return false;

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

std::unordered_map<int, std::shared_ptr<SyncPal>>::const_iterator ExtensionJob::retrieveSyncPalMapIt(const int syncDbId) const {
    const auto result = _commManager->_syncPalMap.find(syncDbId);

    if (result == _commManager->_syncPalMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "SyncPal not found in SyncPalMap - syncDbId=" << syncDbId);
        return _commManager->_syncPalMap.end();
    }

    return result;
}

std::unordered_map<int, std::shared_ptr<Vfs>>::const_iterator ExtensionJob::retrieveVfsMapIt(const int syncDbId) const {
    const auto result = _commManager->_vfsMap.find(syncDbId);
    if (result == _commManager->_vfsMap.cend()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in VfsMap - syncDbId=" << syncDbId);
        return _commManager->_vfsMap.cend();
    }

    return result;
}

ExitInfo ExtensionJob::setPinState(const FileData &fileData, PinState pinState) {
    if (!fileData.syncDbId) return {ExitCode::LogicError, ExitCause::InvalidArgument};

    const auto vfsMapIt = retrieveVfsMapIt(fileData.syncDbId);
    if (vfsMapIt == _commManager->_vfsMap.cend()) return {ExitCode::LogicError};

    return vfsMapIt->second->setPinState(fileData.relativePath, pinState);
}

ExitInfo ExtensionJob::dehydratePlaceholder(const FileData &fileData) {
    if (!fileData.syncDbId) return {ExitCode::LogicError, ExitCause::InvalidArgument};

    const auto vfsMapIt = retrieveVfsMapIt(fileData.syncDbId);
    if (vfsMapIt == _commManager->_vfsMap.cend()) return {ExitCode::LogicError};

    return vfsMapIt->second->dehydratePlaceholder(fileData.relativePath);
}

bool ExtensionJob::addDownloadJob(const FileData &fileData, const SyncPath &parentFolderPath) {
    if (!fileData.syncDbId) return false;

    const auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _commManager->_syncPalMap.end()) return false;

    // Create download job
    const ExitCode exitCode = syncPalMapIt->second->addDlDirectJob(fileData.relativePath, fileData.localPath, parentFolderPath);
    if (exitCode != ExitCode::Ok) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in SyncPal::addDownloadJob - " << Utility::formatSyncPath(fileData.relativePath));
        return false;
    }

    return true;
}

bool ExtensionJob::cancelDownloadJobs(int syncDbId, const std::vector<CommString> &fileList) {
    const auto syncPalMapIt = retrieveSyncPalMapIt(syncDbId);
    if (syncPalMapIt == _commManager->_syncPalMap.end()) return false;

    const auto vfsMapIt = retrieveVfsMapIt(syncDbId);
    if (vfsMapIt == _commManager->_vfsMap.end()) return false;

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

void ExtensionJob::copyUrlToClipboard(const std::string &link) {
    QApplication::clipboard()->setText(QString::fromStdString(link));
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

    // Find SyncPal associated to sync
    auto syncPalMapIt = _commManager->_syncPalMap.end();
    if (fileData.syncDbId) {
        syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    }

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

    // Find SyncPal associated to sync
    auto syncPalMapIt = _commManager->_syncPalMap.end();
    if (fileData.syncDbId) {
        syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    }

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

void ExtensionJob::processFileList(const std::vector<CommString> &inFileList, std::vector<SyncPath> &outFileList) {
    // Process all files
    for (const auto &path: inFileList) {
        FileData fileData = FileData::get(path);
        if (fileData.virtualFileMode == VirtualFileMode::Mac) {
            QFileInfo info(CommString2QStr(path));
            if (info.isDir()) {
                const QFileInfoList infoList =
                        QDir(CommString2QStr(path)).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
                std::vector<CommString> fileList;
                for (const auto &tmpInfo: qAsConst(infoList)) {
                    SyncPath tmpPath(QStr2Path(tmpInfo.filePath()));
                    FileData tmpFileData = FileData::get(tmpPath);

                    auto status = SyncFileStatus::Unknown;
                    if (VfsStatus vfsStatus; !syncFileStatus(tmpFileData, status, vfsStatus)) {
                        LOGW_WARN(Log::instance()->getLogger(),
                                  L"Error in ExtensionJob::syncFileStatus - " << Utility::formatSyncPath(tmpPath));
                        continue;
                    }

                    if (status == SyncFileStatus::Unknown || status == SyncFileStatus::Ignored) {
                        continue;
                    }

                    fileList.push_back(tmpPath);
                }

                if (fileList.size() > 0) {
                    processFileList(fileList, outFileList);
                }
            } else {
                outFileList.push_back(path);
            }
        } else {
            outFileList.push_back(path);
        }
    }
}

CommString ExtensionJob::vfsPinActionText() {
    return QStr2CommString(QObject::tr("Make available locally"));
}

CommString ExtensionJob::vfsFreeSpaceActionText() {
    return QStr2CommString(QObject::tr("Free up local space"));
}

CommString ExtensionJob::cancelDehydrationText() {
    return QStr2CommString(QObject::tr("Cancel free up local space"));
}

CommString ExtensionJob::cancelHydrationText() {
    return QStr2CommString(QObject::tr("Cancel make available locally"));
}

CommString ExtensionJob::resharingText(bool isDirectory) {
    return isDirectory ? QStr2CommString(QObject::tr("Resharing this file is not allowed"))
                       : QStr2CommString(QObject::tr("Resharing this folder is not allowed"));
}

CommString ExtensionJob::copyPublicShareLinkText() {
    return QStr2CommString(QObject::tr("Copy public share link"));
}

CommString ExtensionJob::copyPrivateShareLinkText() {
    return QStr2CommString(QObject::tr("Copy private share link"));
}

CommString ExtensionJob::openInBrowserText() {
    return QStr2CommString(QObject::tr("Open in browser"));
}

bool ExtensionJob::openBrowser(const std::string &url) {
    if (!QDesktopServices::openUrl(QString::fromStdString(url))) {
        LOG_WARN(Log::instance()->getLogger(), "QDesktopServices::openUrl failed - url=" << url);
        return false;
    }
    return true;
}

CommString ExtensionJob::buildMessage(const std::string &verb, const SyncPath &path, const std::string &status) {
    CommString msg(Str2CommString(verb));

    if (!status.empty()) {
        msg.append(responseToFinderArgSeparator);
        msg.append(Str2CommString(status));
    }
    if (!path.empty()) {
        msg.append(responseToFinderArgSeparator);
        msg.append(path.native());
    }
    return msg;
}

void ExtensionJob::monitorFolderHydration(const FileData &fileData) const {
    if (!fileData.syncDbId) return;

    const auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _commManager->_syncPalMap.end()) return;

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
        return FileData();
    }
#else
    SyncPath tmpPath(path);
#endif

    Sync sync;
    if (!syncForPath(tmpPath, sync)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Sync not found - " << Utility::formatSyncPath(tmpPath));
        return FileData();
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
        return FileData();
    }

    if (itemType.ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_DEBUG(Log::instance()->getLogger(), L"Item does not exist anymore - " << Utility::formatSyncPath(tmpPath));
        return FileData();
    }

    data.isLink = itemType.linkType != LinkType::None;
    if (data.isLink) {
        data.isDirectory = false;
    } else {
        std::error_code ec;
        data.isDirectory = std::filesystem::is_directory(tmpPath, ec);
        if (!data.isDirectory && ec.value() != 0) {
            const bool exists = !utility_base::isLikeFileNotFoundError(ec);
            if (!exists) {
                // Item doesn't exist anymore
                LOGW_DEBUG(Log::instance()->getLogger(), L"Item doesn't exist - " << Utility::formatSyncPath(data.localPath));
            } else {
                LOGW_WARN(Log::instance()->getLogger(), L"Failed to check if the path is a directory - "
                                                                << Utility::formatSyncPath(data.localPath) << L" err="
                                                                << CommonUtility::s2ws(ec.message()) << L" (" << ec.value()
                                                                << L")");
            }
            return FileData();
        }
    }

    return data;
}

FileData FileData::parentFolder() const {
    return FileData::get(localPath.parent_path());
}

} // namespace KDC
