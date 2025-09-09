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

#include "libcommon/utility/utility.h"
#include "libcommonserver/vfs/vfs.h"
#include "libcommonserver/vfs/plugin.h"
#include "litesyncextconnector.h"

#include <deque>

#include <QObject>

namespace KDC {

class VfsMac : public Vfs {
        Q_OBJECT
        Q_INTERFACES(KDC::Vfs)

    public:
        explicit VfsMac(const VfsSetupParams &vfsSetupParams, QObject *parent = nullptr);

        VirtualFileMode mode() const override;

        bool socketApiPinStateActionsShown() const override { return true; }

        ExitInfo updateMetadata(const SyncPath &absoluteFilePath, time_t creationTime, time_t modtime, int64_t size,
                                const NodeId &fileId) override;

        ExitInfo createPlaceholder(const SyncPath &relativeLocalPath, const SyncFileItem &item) override;
        ExitInfo dehydratePlaceholder(const SyncPath &relativePath) override;
        ExitInfo convertToPlaceholder(const SyncPath &absolutePath, const SyncFileItem &item) override;
        ExitInfo updateFetchStatus(const SyncPath &tmpPath, const SyncPath &path, int64_t received, bool &canceled,
                                   bool &finished) override;
        ExitInfo updateFetchStatus(const SyncPath &absolutePath, const std::string &status) override;
        ExitInfo forceStatus(const SyncPath &path, const VfsStatus &vfsStatus) override;
        bool cleanUpStatuses() override;
        void clearFileAttributes(const SyncPath &path) override;

        ExitInfo isDehydratedPlaceholder(const SyncPath &relativeFilePath, bool &isDehydrated) override;

        ExitInfo setPinState(const SyncPath &fileRelativePath, PinState state) override;
        PinState pinState(const SyncPath &relativePath) override;
        ExitInfo status(const SyncPath &filePath, VfsStatus &vfsStatus) override;
        void exclude(const SyncPath &path) override;
        bool isExcluded(const SyncPath &filePath) override;
        ExitInfo setThumbnail(const SyncPath &absoluteFilePath, const QPixmap &pixmap) override;
        ExitInfo setAppExcludeList() override;
        ExitInfo getFetchingAppList(QHash<QString, QString> &appTable) override;
        bool fileStatusChanged(const SyncPath &absoluteFilepath, SyncFileStatus status) override;

        void dehydrate(const SyncPath &absoluteFilepath) override;
        void hydrate(const SyncPath &absoluteFilepath) override;
        void cancelHydrate(const SyncPath &absoluteFilepath) override;

        void convertDirContentToPlaceholder(const QString &filePath, bool isHydratedIn) override;

    protected:
        ExitInfo startImpl(bool &installationDone, bool &activationDone, bool &connectionDone) override;
        void stopImpl(bool unregister) override;

        friend class TestWorkers;

    private:
        LiteSyncExtConnector *_connector{nullptr};

        void resetLiteSyncConnector();
};

class MacVfsPluginFactory : public QObject, public DefaultPluginFactory<VfsMac> {
        Q_OBJECT
        Q_PLUGIN_METADATA(IID "org.kdrive.PluginFactory" FILE "../vfspluginmetadata.json")
        Q_INTERFACES(KDC::PluginFactory)
};

} // namespace KDC
