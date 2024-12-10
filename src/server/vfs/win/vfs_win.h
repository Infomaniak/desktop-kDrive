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

        ExitInfo updateMetadata(const QString &filePath, time_t creationTime, time_t modtime, qint64 size,
                                const QByteArray &fileId) final;

        ExitInfo createPlaceholder(const SyncPath &relativeLocalPath, const SyncFileItem &item) final;
        ExitInfo dehydratePlaceholder(const QString &path) final;
        ExitInfo convertToPlaceholder(const QString &path, const SyncFileItem &item) final;
        void convertDirContentToPlaceholder(const QString &filePath, bool isHydratedIn) final;
        void clearFileAttributes(const QString &path) final;

        ExitInfo updateFetchStatus(const QString &tmpPath, const QString &path, qint64 received, bool &canceled,
                                   bool &finished) final;
        ExitInfo forceStatus(const QString &absolutePath, bool isSyncing, int progress, bool isHydrated = false) final;

        ExitInfo isDehydratedPlaceholder(const QString &filePath, bool &isDehydrated, bool isAbsolutePath = false) final;

        ExitInfo setPinState(const QString &fileRelativePath, PinState state) final;
        PinState pinState(const QString &relativePath) final;
        ExitInfo status(const QString &, bool &, bool &, bool &, int &) final;
        ExitInfo setThumbnail(const QString &, const QPixmap &) final { return ExitCode::Ok; };
        ExitInfo setAppExcludeList() final { return ExitCode::Ok; }
        ExitInfo getFetchingAppList(QHash<QString, QString> &) final { return ExitCode::Ok; }

        bool isExcluded(const QString &) final { return false; }
        virtual bool setCreationDate(const QString &, time_t) { return false; }

        void cancelHydrate(const QString &path) final;

        void dehydrate(const QString &path) final;
        void hydrate(const QString &path) final;

    public slots:
        bool fileStatusChanged(const QString &path, KDC::SyncFileStatus status) final;

    protected:
        ExitInfo startImpl(bool &installationDone, bool &activationDone, bool &connectionDone) final;
        void stopImpl(bool unregister) final;

        friend class TestWorkers;

    private:
        log4cplus::Logger _logger;

        void exclude(const QString &path) final;
        ExitInfo setPlaceholderStatus(const QString &path, bool syncOngoing);
};

class WinVfsPluginFactory : public QObject, public DefaultPluginFactory<VfsWin> {
        Q_OBJECT
        Q_PLUGIN_METADATA(IID "org.kdrive.PluginFactory" FILE "../vfspluginmetadata.json")
        Q_INTERFACES(KDC::PluginFactory)
};

} // namespace KDC
