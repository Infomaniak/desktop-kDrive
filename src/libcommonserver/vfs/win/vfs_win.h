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

#include "libcommonserver/vfs/win/syncenginevfslib.h"
#include "libcommonserver/vfs/vfs.h"
#include "libcommonserver/vfs/plugin.h"

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

        VirtualFileMode mode() const override;

        bool socketApiPinStateActionsShown() const override { return false; }

        ExitInfo updateMetadata(const SyncPath &filePath, time_t creationTime, time_t modificationTime, int64_t size,
                                const NodeId &fileId) override;

        ExitInfo createPlaceholder(const SyncPath &relativeLocalPath, const SyncFileItem &item) override;
        ExitInfo dehydratePlaceholder(const SyncPath &path) override;
        ExitInfo convertToPlaceholder(const SyncPath &path, const SyncFileItem &item) override;
        void convertDirContentToPlaceholder(const QString &filePath, bool isHydratedIn) override;
        void clearFileAttributes(const SyncPath &path) override;

        ExitInfo updateFetchStatus(const SyncPath &tmpPath, const SyncPath &path, int64_t received, bool &canceled,
                                   bool &finished) final;
        ExitInfo updateFetchStatus(const SyncPath &, const std::string &) final { return ExitCode::Ok; }
        ExitInfo forceStatus(const SyncPath &absolutePath, const VfsStatus &vfsStatus) final;

        ExitInfo isDehydratedPlaceholder(const SyncPath &filePath, bool &isDehydrated) override;

        ExitInfo setPinState(const SyncPath &fileRelativePath, PinState state) final;
        PinState pinState(const SyncPath &relativePath) final;
        ExitInfo status(const SyncPath &, VfsStatus &) final;
        ExitInfo setThumbnail(const SyncPath &, const QPixmap &) final { return ExitCode::Ok; };
        ExitInfo setAppExcludeList() final { return ExitCode::Ok; }
        ExitInfo getFetchingAppList(QHash<QString, QString> &) final { return ExitCode::Ok; }

        bool isExcluded(const SyncPath &) override { return false; }
        virtual bool setCreationDate(const QString &, time_t) { return false; }

        void dehydrate(const SyncPath &path) override;
        void hydrate(const SyncPath &path) override;
        void cancelHydrate(const SyncPath &path) override;


    public slots:
        bool fileStatusChanged(const SyncPath &path, KDC::SyncFileStatus status) override;

    protected:
        ExitInfo startImpl(bool &installationDone, bool &activationDone, bool &connectionDone) override;
        void stopImpl(bool unregister) override;

        friend class TestWorkers;

    private:
        log4cplus::Logger _logger;

        void exclude(const SyncPath &path) override;
        ExitInfo setPlaceholderStatus(const SyncPath &path, bool syncOngoing);
};

class WinVfsPluginFactory : public QObject, public DefaultPluginFactory<VfsWin> {
        Q_OBJECT
        Q_PLUGIN_METADATA(IID "org.kdrive.PluginFactory" FILE "../vfspluginmetadata.json")
        Q_INTERFACES(KDC::PluginFactory)
};

} // namespace KDC
