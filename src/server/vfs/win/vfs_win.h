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

#include <QList>
#include <QMutex>
#include <QObject>
#include <QScopedPointer>
#include <QThread>
#include <QWaitCondition>

#define WORKER_HYDRATION 0
#define WORKER_DEHYDRATION 1
#define NB_WORKERS 2

namespace KDC {

class Worker;

struct WorkerInfo {
        QMutex _mutex;
        std::deque<QString> _queue;
        QWaitCondition _queueWC;
        bool _stop = false;
        QList<QThread *> _threadList;
};

class SYNCENGINEVFS_EXPORT VfsWin : public Vfs {
        Q_OBJECT
        Q_INTERFACES(KDC::Vfs)

    public:
        WorkerInfo _workerInfo[NB_WORKERS];

        explicit VfsWin(VfsSetupParams &vfsSetupParams, QObject *parent = nullptr);
        ~VfsWin();

        void debugCbk(TraceLevel level, const wchar_t *msg);

        VirtualFileMode mode() const override;

        bool socketApiPinStateActionsShown() const override { return false; }

        ExitInfo updateMetadata(const QString &filePath, time_t creationTime, time_t modtime, qint64 size,
                                const QByteArray &fileId,
                            QString *error) override;

        ExitInfo createPlaceholder(const SyncPath &relativeLocalPath, const SyncFileItem &item) override;
        ExitInfo dehydratePlaceholder(const QString &path) override;
        ExitInfo convertToPlaceholder(const QString &path, const SyncFileItem &item) override;
        void convertDirContentToPlaceholder(const QString &filePath, bool isHydratedIn) override;
        virtual void clearFileAttributes(const QString &path) override;

        ExitInfo updateFetchStatus(const QString &tmpPath, const QString &path, qint64 received, bool &canceled,
                               bool &finished) override;
        ExitInfo forceStatus(const QString &absolutePath, bool isSyncing, int progress, bool isHydrated = false) override;

        bool isDehydratedPlaceholder(const QString &filePath, bool isAbsolutePath = false) override;

        ExitInfo setPinState(const QString &fileRelativePath, PinState state) override;
        PinState pinState(const QString &relativePath) override;
        ExitInfo status(const QString &, bool &, bool &, bool &, int &) override;
        virtual ExitInfo setThumbnail(const QString &, const QPixmap &) override { return ExitCode::Ok; };
        virtual ExitInfo setAppExcludeList() override { return ExitCode::Ok; }
        virtual ExitInfo getFetchingAppList(QHash<QString, QString> &) override { return ExitCode::Ok; }

        bool isExcluded(const QString &) override { return false; }
        virtual bool setCreationDate(const QString &, time_t) { return false; }

        virtual void cancelHydrate(const QString &path) override;

        void dehydrate(const QString &path);
        void hydrate(const QString &path);

    public slots:
        bool fileStatusChanged(const QString &path, KDC::SyncFileStatus status) override;

    protected:
        ExitInfo startImpl(bool &installationDone, bool &activationDone, bool &connectionDone) override;
        void stopImpl(bool unregister) override;

        friend class TestWorkers;

    private:
        log4cplus::Logger _logger;

        void exclude(const QString &path) override;
        void setPlaceholderStatus(const QString &path, bool syncOngoing);
};

class Worker : public QObject {
        Q_OBJECT

    public:
        Worker(VfsWin *vfs, int type, int num, log4cplus::Logger logger);
        void start();

    private:
        VfsWin *_vfs;
        int _type;
        int _num;
        log4cplus::Logger _logger;

        inline log4cplus::Logger logger() { return _logger; }
};

class WinVfsPluginFactory : public QObject, public DefaultPluginFactory<VfsWin> {
        Q_OBJECT
        Q_PLUGIN_METADATA(IID "org.kdrive.PluginFactory" FILE "../vfspluginmetadata.json")
        Q_INTERFACES(KDC::PluginFactory)
};

} // namespace KDC
