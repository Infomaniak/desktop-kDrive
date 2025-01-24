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

#include "server/vfs/win/syncenginevfslib.h"
#include "libcommonserver/vfs.h"
#include "libcommonserver/plugin.h"

#include <deque>

#include <windows.h>

// Vfs dll
#include "debug.h"
#include "vfs.h"

namespace KDC {

class SYNCENGINEVFS_EXPORT VfsWin : public Vfs {
        Q_OBJECT
        Q_INTERFACES(KDC::Vfs)

    public:
        explicit VfsWin(const VfsSetupParams &vfsSetupParams, QObject *parent = nullptr);

        void debugCbk(TraceLevel level, const wchar_t *msg);

        VirtualFileMode mode() const final;

        bool socketApiPinStateActionsShown() const final { return false; }

        ExitInfo updateMetadata(const SyncPath &filePath, time_t creationTime, time_t modtime, int64_t size,
                                const NodeId &fileId) final;

        ExitInfo createPlaceholder(const SyncPath &relativeLocalPath, const SyncFileItem &item) final;
        ExitInfo dehydratePlaceholder(const SyncPath &path) final;
        ExitInfo convertToPlaceholder(const SyncPath &path, const SyncFileItem &item) final;
        void convertDirContentToPlaceholder(const QString &filePath, bool isHydratedIn) final;
        void clearFileAttributes(const SyncPath &path) final;

        ExitInfo updateFetchStatus(const SyncPath &tmpPath, const SyncPath &path, int64_t received, bool &canceled,
                                   bool &finished) final;
        ExitInfo forceStatus(const SyncPath &absolutePath, bool isSyncing, int progress, bool isHydrated = false) final;

        ExitInfo isDehydratedPlaceholder(const SyncPath &filePath, bool &isDehydrated, bool isAbsolutePath = false) final;

        ExitInfo setPinState(const SyncPath &fileRelativePath, PinState state) final;
        PinState pinState(const SyncPath &relativePath) final;
        ExitInfo status(const SyncPath &, bool &, bool &, bool &, int &) final;
        ExitInfo setThumbnail(const SyncPath &, const QPixmap &) final { return ExitCode::Ok; };
        ExitInfo setAppExcludeList() final { return ExitCode::Ok; }
        ExitInfo getFetchingAppList(QHash<QString, QString> &) final { return ExitCode::Ok; }

        bool isExcluded(const SyncPath &) final { return false; }
        virtual bool setCreationDate(const QString &, time_t) { return false; }

        void dehydrate(const SyncPath &path) final;
        void hydrate(const SyncPath &path) final;
        void cancelHydrate(const SyncPath &path) final;


    public slots:
        bool fileStatusChanged(const SyncPath &path, KDC::SyncFileStatus status) final;

    protected:
        ExitInfo startImpl(bool &installationDone, bool &activationDone, bool &connectionDone) final;
        void stopImpl(bool unregister) final;

        friend class TestWorkers;

    private:
        log4cplus::Logger _logger;

        void exclude(const SyncPath &path) final;
        ExitInfo setPlaceholderStatus(const SyncPath &path, bool syncOngoing);
};

class WinVfsPluginFactory : public QObject, public DefaultPluginFactory<VfsWin> {
        Q_OBJECT
        Q_PLUGIN_METADATA(IID "org.kdrive.PluginFactory" FILE "../vfspluginmetadata.json")
        Q_INTERFACES(KDC::PluginFactory)
};

} // namespace KDC
