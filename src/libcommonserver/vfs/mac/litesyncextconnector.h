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

#include "libcommon/utility/types.h"
#include "libcommonserver/log/log.h"
#include "vfs/vfs.h"

#include <sys/stat.h>

#include <QMap>
#include <QHash>
#include <QSet>
#include <QString>

namespace KDC {

class LiteSyncExtConnectorPrivate;

class LiteSyncExtConnector {
    public:
        LiteSyncExtConnector(LiteSyncExtConnector &other) = delete;
        void operator=(const LiteSyncExtConnector &) = delete;
        static LiteSyncExtConnector *instance(log4cplus::Logger logger, ExecuteCommand executeCommand);
        static bool vfsGetStatus(const SyncPath &absoluteFilePath, VfsStatus &vfsStatus, log4cplus::Logger &logger) noexcept;

        ~LiteSyncExtConnector();

        bool install(bool &activationDone);
        bool connect();

        bool vfsStart(int syncDbId, const SyncPath &folderPath, bool &isPlaceholder, bool &isSyncing);
        bool vfsStop(int syncDbId);
        bool vfsDehydratePlaceHolder(const QString &absoluteFilepath, const QString &localSyncPath);
        bool vfsHydratePlaceHolder(const SyncPath &filePath);
        bool vfsSetPinState(const QString &path, const QString &localSyncPath, const std::string_view &pinState);
        bool vfsGetPinState(const SyncPath &path, std::string &pinState);
        bool vfsConvertToPlaceHolder(const QString &filePath, bool isHydrated);
        bool vfsCreatePlaceHolder(const QString &relativePath, const QString &localSyncPath, const struct stat *fileStat);
        bool vfsUpdateFetchStatus(const QString &tmpFilePath, const QString &filePath, const QString &localSyncPath,
                                  unsigned long long completed, bool &canceled, bool &finished);
        bool vfsUpdateFetchStatus(const QString &absolutePath, const QString &status);
        bool vfsCancelHydrate(const QString &filePath);
        bool vfsSetThumbnail(const QString &absoluteFilePath, const QPixmap &pixmap);
        bool vfsSetStatus(const QString &path, const QString &localSyncPath, const VfsStatus &vfsStatus);
        bool vfsCleanUpStatuses(const QString &localSyncPath);
        bool vfsGetStatus(const SyncPath &absoluteFilePath, VfsStatus &vfsStatus) noexcept {
            return vfsGetStatus(absoluteFilePath, vfsStatus, _logger);
        }
        bool vfsSetAppExcludeList(const QString &appList);
        bool vfsGetFetchingAppList(QHash<QString, QString> &appTable);
        bool vfsUpdateMetadata(const QString &absoluteFilePath, const struct stat *fileStat);
        bool vfsIsExcluded(const QString &path);
        bool vfsProcessDirStatus(const QString &path, const QString &localSyncPath);
        void vfsClearFileAttributes(const QString &path);
        bool checkFilesAttributes(const QString &path, const QString &localSyncPath, QStringList &filesToFix);

        void resetConnector(log4cplus::Logger logger, ExecuteCommand executeCommand);

    protected:
        LiteSyncExtConnector(log4cplus::Logger logger, ExecuteCommand executeCommand);

        static LiteSyncExtConnector *_liteSyncExtConnector;

    private:
        log4cplus::Logger _logger;
        LiteSyncExtConnectorPrivate *_private{nullptr};
        QMap<int, SyncPath> _folders;

        /**
         * Keeps track of folder with `syncing` status.
         * Key: parent folder path.
         * Value: List of syncing items path.
         */
        QHash<QString, QSet<QString>> _syncingFolders;
        std::recursive_mutex _mutex;

        bool sendStatusToFinder(const QString &path, const VfsStatus &vfsStatus);
};

} // namespace KDC
