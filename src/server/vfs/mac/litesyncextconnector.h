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

#include "libcommon/utility/types.h"
#include "libcommonserver/log/log.h"

#include <sys/stat.h>

#include <QMap>
#include <QPixmap>
#include <QString>

// EXT_ATTR_STATUS clone
#define VFS_STATUS_ONLINE "O"
#define VFS_STATUS_OFFLINE "F"
#define VFS_STATUS_HYDRATING "H"

// EXT_ATTR_PIN_STATE clone
#define VFS_PIN_STATE_UNPINNED "U"
#define VFS_PIN_STATE_PINNED "P"
#define VFS_PIN_STATE_EXCLUDED "E"

namespace KDC {

class LiteSyncExtConnectorPrivate;

class LiteSyncExtConnector {
    public:
        LiteSyncExtConnector(LiteSyncExtConnector &other) = delete;
        void operator=(const LiteSyncExtConnector &) = delete;
        static LiteSyncExtConnector *instance(log4cplus::Logger logger, ExecuteCommand executeCommand);

        ~LiteSyncExtConnector();

        bool install(bool &activationDone);
        bool connect();

        bool vfsStart(int syncDbId, const QString &folderPath, bool &isPlaceholder, bool &isSyncing);
        bool vfsStop(int syncDbId);
        bool vfsDehydratePlaceHolder(const QString &absoluteFilepath);
        bool vfsHydratePlaceHolder(const QString &filePath);
        bool vfsSetPinState(const QString &path, const QString &pinState);
        bool vfsGetPinState(const QString &path, QString &pinState);
        bool vfsConvertToPlaceHolder(const QString &filePath, bool isHydrated);
        bool vfsCreatePlaceHolder(const QString &relativePath, const QString &destPath, const struct stat *fileStat);
        bool vfsUpdateFetchStatus(const QString &tmpFilePath, const QString &filePath, unsigned long long completed,
                                  bool &canceled, bool &finished);
        bool vfsSetThumbnail(const QString &absoluteFilePath, const QPixmap &pixmap);
        bool vfsSetStatus(const QString &path, bool isSyncing, int progress, bool isHydrated = false);
        bool vfsGetStatus(const QString &absoluteFilePath, bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress);
        bool vfsSetAppExcludeList(const QString &appList);
        bool vfsGetFetchingAppList(QHash<QString, QString> &appTable);
        bool vfsUpdateMetadata(const QString &absoluteFilePath, const struct stat *fileStat, QString *error);
        bool vfsIsExcluded(const QString &path);
        bool vfsProcessDirStatus(const QString &path);
        void vfsClearFileAttributes(const QString &path);
        bool checkFilesAttributes(const QString &path, QStringList &filesToFix);

        void resetConnector(log4cplus::Logger logger, ExecuteCommand executeCommand);

    protected:
        LiteSyncExtConnector(log4cplus::Logger logger, ExecuteCommand executeCommand);

        static LiteSyncExtConnector *_liteSyncExtConnector;

    private:
        log4cplus::Logger _logger;
        LiteSyncExtConnectorPrivate *_private;
        QMap<int, QString> _folders;

        bool sendStatusToFinder(const QString &path, bool isSyncing, int progress, bool isHydrated);
};

}  // namespace KDC
