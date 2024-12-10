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

#include "libcommon/utility/utility.h"
#include "libcommonserver/vfs.h"
#include "libcommonserver/plugin.h"
#include "litesyncextconnector.h"

#include <deque>

#include <QList>
#include <QMutex>
#include <QObject>
#include <QScopedPointer>
#include <QWaitCondition>

#define WORKER_HYDRATION 0
#define WORKER_DEHYDRATION 1

namespace KDC {

class Worker;

class VfsMac : public Vfs {
        Q_OBJECT
        Q_INTERFACES(KDC::Vfs)

    public:
        explicit VfsMac(KDC::VfsSetupParams &vfsSetupParams, QObject *parent = nullptr);

        VirtualFileMode mode() const override;

        bool socketApiPinStateActionsShown() const override { return true; }

        ExitInfo updateMetadata(const QString &absoluteFilePath, time_t creationTime, time_t modtime, qint64 size,
                            const QByteArray &fileId) override;

        ExitInfo createPlaceholder(const SyncPath &relativeLocalPath, const SyncFileItem &item) override;
        ExitInfo dehydratePlaceholder(const QString &path) override;
        ExitInfo convertToPlaceholder(const QString &path, const SyncFileItem &item) override;
        ExitInfo updateFetchStatus(const QString &tmpPath, const QString &path, qint64 received, bool &canceled,
                               bool &finished) override;
        void cancelHydrate(const QString &filePath) override;
        ExitInfo forceStatus(const QString &path, bool isSyncing, int progress, bool isHydrated = false) override;
        bool cleanUpStatuses() override;
        virtual void clearFileAttributes(const QString &path) override;

        ExitInfo isDehydratedPlaceholder(const QString &filePath, bool &isDehydrated, bool isAbsolutePath = false) override;

        ExitInfo setPinState(const QString &fileRelativePath, PinState state) override;
        PinState pinState(const QString &relativePath) override;
        ExitInfo status(const QString &filePath, bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) override;
        void exclude(const QString &path) override;
        bool isExcluded(const QString &filePath) override;
        ExitInfo setThumbnail(const QString &absoluteFilePath, const QPixmap &pixmap) override;
        ExitInfo setAppExcludeList() override;
        ExitInfo getFetchingAppList(QHash<QString, QString> &appTable) override;
        bool fileStatusChanged(const QString &path, SyncFileStatus status) override;

        void dehydrate(const QString &path);
        void hydrate(const QString &path);

        virtual void convertDirContentToPlaceholder(const QString &filePath, bool isHydratedIn) override;

    protected:
        ExitInfo startImpl(bool &installationDone, bool &activationDone, bool &connectionDone) override;
        void stopImpl(bool unregister) override;

        friend class TestWorkers;

    private:
        LiteSyncExtConnector *_connector{nullptr};

        void resetLiteSyncConnector();
        const QString _localSyncPath;
};

class Worker : public QObject {
        Q_OBJECT

    public:
        Worker(VfsMac *vfs, int type, int num, log4cplus::Logger logger);
        void start();

    private:
        VfsMac *_vfs;
        int _type;
        int _num;
        log4cplus::Logger _logger;

        inline log4cplus::Logger logger() { return _logger; }
};

class MacVfsPluginFactory : public QObject, public DefaultPluginFactory<VfsMac> {
        Q_OBJECT
        Q_PLUGIN_METADATA(IID "org.kdrive.PluginFactory" FILE "../vfspluginmetadata.json")
        Q_INTERFACES(KDC::PluginFactory)
};

} // namespace KDC
