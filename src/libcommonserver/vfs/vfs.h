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
#include "libcommon/utility/utility.h"
#include "libcommon/log/sentry/handler.h"
#include "libcommon/utility/sourcelocation.h"
#include "libcommonserver/vfs/workerinfo.h"
#include "libsyncengine/progress/syncfileitem.h"

#include <memory>
#include <deque>

#include <QObject>

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

static constexpr size_t nbWorkers = 2;
static constexpr short workerHydration = 0;
static constexpr short workerDehydration = 1;

namespace KDC {

struct VfsSetupParams {
        VfsSetupParams() = default;
        explicit VfsSetupParams(const log4cplus::Logger &logger) :
            logger(logger) {}
        int syncDbId{-1};
        int driveId{-1};
        int userId{-1};
        SyncPath localPath;
        SyncPath targetPath;
        std::string namespaceCLSID;
        ExecuteCommand executeCommand;
        log4cplus::Logger logger;
        std::shared_ptr<sentry::Handler> sentryHandler;
};

struct VfsStatus {
        bool isPlaceholder{false};
        bool isHydrated{false};
        bool isSyncing{false};
        int16_t progress{0};

        VfsStatus &operator=(const VfsStatus &other) {
            if (this == &other) return *this;
            isPlaceholder = other.isPlaceholder;
            isHydrated = other.isHydrated;
            isSyncing = other.isSyncing;
            progress = other.progress;
            return *this;
        }
};

/** Interface describing how to deal with virtual/placeholder files.
 *
 * There are different ways of representing files locally that will only
 * be filled with data (hydrated) on demand. One such way would be suffixed
 * files, others could be FUSE based or use Windows CfAPI.
 *
 * This interface intends to decouple the sync algorithm and Folder from
 * the details of how a particular VFS solution works.
 *
 * An instance is usually created through a plugin via the createVfsFromPlugin()
 * function.
 */
class Vfs : public QObject {
        Q_OBJECT

    public:
        std::array<WorkerInfo, nbWorkers> _workerInfo;
        static QString modeToString(VirtualFileMode virtualFileMode);
        static VirtualFileMode modeFromString(const QString &str);

        explicit Vfs(const VfsSetupParams &vfsSetupParams, QObject *parent = nullptr);

        ~Vfs() override;

        void setSyncFileStatusCallback(void (*syncFileStatus)(int, const SyncPath &, SyncFileStatus &)) {
            _syncFileStatus = syncFileStatus;
        }
        void setSyncFileSyncingCallback(void (*syncFileSyncing)(int, const SyncPath &, bool &)) {
            _syncFileSyncing = syncFileSyncing;
        }
        void setSetSyncFileSyncingCallback(void (*setSyncFileSyncing)(int, const SyncPath &, bool)) {
            _setSyncFileSyncing = setSyncFileSyncing;
        }
        void setExclusionAppListCallback(void (*exclusionAppList)(QString &)) { _exclusionAppList = exclusionAppList; }

        virtual VirtualFileMode mode() const = 0;

        /** Initializes interaction with the VFS provider.
         *
         * The plugin-specific work is done in startImpl().
         * Possible return values are:
         * - ExitCode::Ok: Everything went fine.
         * - ExitCode::LiteSyncError, ExitCause::UnableToCreateVfs: The VFS provider could not be started.
         */
        ExitInfo start(bool &installationDone, bool &activationDone, bool &connectionDone);

        /// Stop interaction with VFS provider. Like when the client application quits.
        /// Also deregister the folder with the sync provider, like when a folder is removed.
        void stop(bool unregister);

        /** Whether the socket api should show pin state options
         *
         * Some plugins might provide alternate shell integration, making the normal
         * context menu actions redundant.
         */
        virtual bool socketApiPinStateActionsShown() const = 0;

        /** Update placeholder metadata.
         *
         * If the remote metadata changes, the local placeholder's metadata should possibly
         * change as well.
         *
         * Possible return values are:
         * - ExitCode::Ok: Everything went fine, the metadata was updated.
         * - ExitCode::LogicError, ExitCause::Unknown: The liteSync connector is not initialized.
         * - ExitCode::SystemError, ExitCause::Unknown: An unknown error occurred.
         * - ExitCode::SystemError, ExitCause::NotFound: The item(filePath) could not be found on the local filesystem.
         * - ExitCode::SystemError, ExitCause::FileAccessError: Missing permissions on the item(filePath) or the
         * item(filePath) is locked.
         */
        virtual ExitInfo updateMetadata(const SyncPath &filePath, time_t creationTime, time_t modtime, int64_t size,
                                        const NodeId &fileId) = 0;

        /** Create a new dehydrated placeholder
         *
         * Possible return values are:
         * - ExitCode::Ok: Everything went fine, the placeholder was created.
         * - ExitCode::LogicError, ExitCause::InvalidArgument: relativeLocalPath is empty or item.remoteNodeId is not set.
         * - ExitCode::SystemError, ExitCause::Unknown: An unknown error occurred.
         * - ExitCode::SystemError, ExitCause::NotFound: The parent folder does not exist.
         * - ExitCode::SystemError, ExitCause::FileAccessError: Missing permissions on the destination folder.
         * - ExitCode::SystemError, ExitCause::FileAlreadyExist: An item with the same name already exists in the
         * destination folder.
         */
        virtual ExitInfo createPlaceholder(const SyncPath &relativeLocalPath, const SyncFileItem &item) = 0;

        /** Convert a hydrated placeholder to a dehydrated one.
         * * Possible return values are:
         * - ExitCode::Ok: Everything went fine, the placeholder is dehydrating (async).
         * - ExitCode::LogicError, ExitCause::InvalidArgument: The provided path is empty.
         * - ExitCode::SystemError, ExitCause::Unknown: An unknown error occurred.
         * - ExitCode::SystemError, ExitCause::NotFound: The item could not be found.
         * - ExitCode::SystemError, ExitCause::FileAccessError: Missing permissions on the item ot the item is locked.
         * - ExitCode::SystemError, ExitCause::NotPlaceHolder: The item is not a placeholder.
         * folder.
         */
        virtual ExitInfo dehydratePlaceholder(const SyncPath &path) = 0;

        /** Convert a new file to a hydrated placeholder.
         *
         * Some VFS integrations expect that every file, including those that have all
         * the remote data, are "placeholders".
         *
         * Implementations must make sure that calling this function on a file that already
         * is a placeholder is acceptable.
         *
         * * Possible return values are:
         * - ExitCode::Ok: Everything went fine, the file is now a placeholder.
         * - ExitCode::LogicError, ExitCause::InvalidArgument: item.localNodeId is not set.
         * - ExitCode::SystemError, ExitCause::Unknown: An unknown error occurred.
         * - ExitCode::SystemError, ExitCause::NotFound: The item could not be found.
         * - ExitCode::SystemError, ExitCause::FileAccessError: Missing permissions on the item ot the item is locked.
         */
        virtual ExitInfo convertToPlaceholder(const SyncPath &path, const SyncFileItem &item) = 0;

        /** Update the fetch status of a file.
         *
         * This is used to update the progress of a file download in the OS file UI.
         *
         * * Possible return values are:
         * - ExitCode::Ok: Everything went fine, the fetch status was updated.
         * - ExitCode::SystemError, ExitCause::Unknown: An unknown error occurred.
         * - ExitCode::SystemError, ExitCause::NotFound: The item could not be found.
         * - ExitCode::SystemError, ExitCause::FileAccessError: Missing permissions on the item ot the item is locked (The
         * item is the file in the sync folder, any error on the tmpItem will lead to SystemError, Unknown).
         */
        virtual ExitInfo updateFetchStatus(const SyncPath &tmpPath, const SyncPath &path, int64_t received, bool &canceled,
                                           bool &finished) = 0;
        virtual ExitInfo updateFetchStatus(const SyncPath &, const std::string &) = 0;

        /** Force the status of a file.
         *
         * This is used to force the sync status of a file (isSyncing or not)
         *
         * * Possible return values are:
         * - ExitCode::Ok: Everything went fine, the status was updated.
         * - ExitCode::SystemError, ExitCause::Unknown: An unknown error occurred.
         * - ExitCode::SystemError, ExitCause::NotFound: The item could not be found.
         * - ExitCode::SystemError, ExitCause::FileAccessError: Missing permissions on the item ot the item is locked.
         */
        virtual ExitInfo forceStatus(const SyncPath &path, const VfsStatus &vfsStatus) = 0;

        virtual bool cleanUpStatuses() { return true; }

        /** Determine whether the file at the given path is a dehydrated placeholder.
         *
         * * Possible return values are:
         * - ExitCode::Ok: Everything went fine, isDehydrated is set to true if the file is a dehydrated placeholder.
         * - ExitCode::SystemError, ExitCause::Unknown: An unknown error occurred.
         * - ExitCode::SystemError, ExitCause::NotFound: The item could not be found.
         * - ExitCode::SystemError, ExitCause::FileAccessError: Missing permissions on the item ot the item is locked.
         */
        virtual ExitInfo isDehydratedPlaceholder(const SyncPath &filePath, bool &isDehydrated) = 0;

        /** Sets the pin state for the item at a path.
         *
         * The pin state is set on the item and for all items below it.
         *
         * Usually this would forward to setting the pin state flag in the db table,
         * but some vfs plugins will store the pin state in file attributes instead.
         *
         * fileRelativePath is relative to the sync folder. Can be "" for root folder.
         *
         * * Possible return values are:
         * - ExitCode::Ok: Everything went fine, the pin state was updated.
         * - ExitCode::SystemError, ExitCause::Unknown: An unknown error occurred.
         * - ExitCode::SystemError, ExitCause::NotFound: The item could not be found.
         * - ExitCode::SystemError, ExitCause::FileAccessError: Missing permissions on the item ot the item is locked.
         */
        virtual ExitInfo setPinState(const SyncPath &fileRelativePath, PinState state) = 0;

        /** Returns the pin state of an item at a path.
         *
         * Usually backed by the db's effectivePinState() function but some vfs
         * plugins will override it to retrieve the state from elsewhere.
         *
         * fileRelativePath is relative to the sync folder. Can be "" for root folder.
         *
         * Returns none on retrieval error.
         */
        virtual PinState pinState(const SyncPath &fileRelativePath) = 0;

        /** Returns the status of a file.
         *
         * * Possible return values are:
         * - ExitCode::Ok: Everything went fine, the status was retrieved.
         * - ExitCode::SystemError, ExitCause::Unknown: An unknown error occurred.
         * - ExitCode::SystemError, ExitCause::NotFound: The item could not be found.
         * - ExitCode::SystemError, ExitCause::FileAccessError: Missing permissions on the item ot the item is locked.
         */
        virtual ExitInfo status(const SyncPath &filePath, VfsStatus &vfsStatus) = 0;

        /** Set the thumbnail for a file.
         *
         * * Possible return values are:
         * - ExitCode::Ok: Everything went fine, the status was retrieved.
         * - ExitCode::SystemError, ExitCause::Unknown: An unknown error occurred.
         * - ExitCode::SystemError, ExitCause::NotFound: The item could not be found.
         * - ExitCode::SystemError, ExitCause::FileAccessError: Missing permissions on the item ot the item is locked.
         */
        virtual ExitInfo setThumbnail(const SyncPath &filePath, const QPixmap &pixmap) = 0;

        /** Set the list of applications that should not be hydrated.
         *
         * * Possible return values are:
         * - ExitCode::Ok: Everything went fine, the list was set.
         * - ExitCode::LogicError, ExitCause::Unknown: An unknown error occurred.
         */
        virtual ExitInfo setAppExcludeList() = 0;

        /** Set the list of applications that should not be hydrated.
         *
         * * Possible return values are:
         * - ExitCode::Ok: Everything went fine, the list was set.
         * - ExitCode::LogicError, ExitCause::Unknown: An unknown error occurred.
         */
        virtual ExitInfo getFetchingAppList(QHash<QString, QString> &appTable) = 0;

        virtual void exclude(const SyncPath &) = 0;
        virtual bool isExcluded(const SyncPath &filePath) = 0;


        virtual bool fileStatusChanged(const SyncPath &systemFileName, SyncFileStatus fileStatus) = 0;

        virtual void convertDirContentToPlaceholder(const QString &, bool) {}

        virtual void clearFileAttributes(const SyncPath &) = 0;

        void setExtendedLog(bool extendedLog) { _extendedLog = extendedLog; }

        const std::string &namespaceCLSID() { return _vfsSetupParams.namespaceCLSID; }
        void setNamespaceCLSID(const std::string &CLSID) { _vfsSetupParams.namespaceCLSID = CLSID; }

        virtual void dehydrate(const SyncPath &path) = 0;
        virtual void hydrate(const SyncPath &path) = 0;
        virtual void cancelHydrate(const SyncPath &) = 0;

    signals:
        /// Emitted when a user-initiated hydration starts
        void beginHydrating();
        /// Emitted when the hydration ends
        void doneHydrating();

    protected:
        VfsSetupParams _vfsSetupParams;
        void starVfsWorkers();
        const std::array<size_t, nbWorkers> s_nb_threads = {5, 5};

        // Callbacks
        void (*_syncFileStatus)(int syncDbId, const KDC::SyncPath &itemPath, KDC::SyncFileStatus &status) = nullptr;
        void (*_syncFileSyncing)(int syncDbId, const KDC::SyncPath &itemPath, bool &syncing) = nullptr;
        void (*_setSyncFileSyncing)(int syncDbId, const KDC::SyncPath &itemPath, bool syncing) = nullptr;
        void (*_exclusionAppList)(QString &appList) = nullptr;

        bool extendedLog() { return _extendedLog; }

        /** Set up the plugin for the folder.
         *
         * For example, the VFS provider might monitor files to be able to start a file
         * hydration (download of a file's remote contents) when the user wants to open
         * it.
         *
         * Usually some registration needs to be done with the backend. This function
         * should take care of it if necessary.
         */
        virtual ExitInfo startImpl(bool &installationDone, bool &activationDone, bool &connectionDone) = 0;

        virtual void stopImpl(bool unregister) = 0;

        log4cplus::Logger logger() const { return _vfsSetupParams.logger; }


        /* Handle a VFS error by logging it and returning an ExitInfo with the appropriate error code.
         *  As the reason behind the error is not (yet) provided by some of the OS specific VFS implementations,
         *  the error provided to the application will only be based on the existence/permission of the file/directory.
         *  If there is no issue with the file/directory, the error will be Vfs::defaultVfsError().         *
         */
        ExitInfo handleVfsError(const SyncPath &itemPath, const SourceLocation &location = SourceLocation::currentLoc()) const;

        /* Check if a path exists and return an ExitInfo with the appropriate error code.
         *
         * @param itemPath The path to check.
         * @param shouldExist Whether the path should exist or not.
         * @param location The location of the call.
         *
         * @return ExitInfo with the appropriate error code:
         *   - ExitCode::Ok if the path exists and shouldExist is true.
         *   - ExitCode::Ok if the path does not exist and shouldExist is false.
         *   - ExitCode::SystemError, ExitCause::NotFound if the path does not exist and shouldExist is true.
         *   - ExitCode::SystemError, ExitCause::FileAlreadyExist if the path exist and shouldExist is false.
         *   - ExitCode::SystemError, ExitCause::FileAccessError if the path is not accessible.
         *   - ExitCode::SystemError, ExitCause::InvalidArguments if the path is empty.
         */
        ExitInfo checkIfPathIsValid(const SyncPath &itemPath, bool shouldExist,
                                    const SourceLocation &location = SourceLocation::currentLoc()) const;

        /* By default, we will return file access error.
         *  The file will be blacklisted for 1h or until the user edit, move or delete it (or the sync is restarted).
         */
        ExitInfo defaultVfsError(const SourceLocation &location = SourceLocation::currentLoc()) const {
            return {ExitCode::SystemError, ExitCause::FileAccessError, location};
        }

    private:
        bool _extendedLog;
        bool _started;
};
} // namespace KDC

Q_DECLARE_INTERFACE(KDC::Vfs, "Vfs")

//
// VfsOff
//

namespace KDC {
/// Implementation of Vfs for Vfs::Off mode - does nothing
class VfsOff : public Vfs {
        Q_OBJECT
        Q_INTERFACES(KDC::Vfs)

    public:
        explicit VfsOff(QObject *parent = nullptr);
        VfsOff(const VfsSetupParams &vfsSetupParams, QObject *parent = nullptr);

        ~VfsOff() override;

        VirtualFileMode mode() const override { return VirtualFileMode::Off; }

        bool socketApiPinStateActionsShown() const override { return false; }

        ExitInfo updateMetadata(const SyncPath &, time_t, time_t, int64_t, const NodeId &) override { return ExitCode::Ok; }
        ExitInfo createPlaceholder(const SyncPath &, const SyncFileItem &) override { return ExitCode::Ok; }
        ExitInfo dehydratePlaceholder(const SyncPath &) override { return ExitCode::Ok; }
        ExitInfo convertToPlaceholder(const SyncPath &, const SyncFileItem &) override { return ExitCode::Ok; }
        ExitInfo updateFetchStatus(const SyncPath &, const SyncPath &, int64_t, bool &, bool &) override { return ExitCode::Ok; }
        ExitInfo updateFetchStatus(const SyncPath &, const std::string &) override { return ExitCode::Ok; }
        ExitInfo forceStatus(const SyncPath &path, const VfsStatus &vfsStatus) override;

        ExitInfo isDehydratedPlaceholder(const SyncPath &, bool &isDehydrated) override {
            isDehydrated = false;
            return ExitCode::Ok;
        }

        ExitInfo setPinState(const SyncPath &, PinState) override { return ExitCode::Ok; }
        PinState pinState(const SyncPath &) override { return PinState::AlwaysLocal; }
        ExitInfo status(const SyncPath &, VfsStatus &vfsStatus) override {
            vfsStatus.isPlaceholder = false;
            vfsStatus.isHydrated = true;
            return ExitCode::Ok;
        }
        ExitInfo setThumbnail(const SyncPath &, const QPixmap &) override { return ExitCode::Ok; }
        ExitInfo setAppExcludeList() override { return ExitCode::Ok; }
        ExitInfo getFetchingAppList(QHash<QString, QString> &) override { return ExitCode::Ok; }
        void exclude(const SyncPath &) override { /*VfsOff*/ }
        bool isExcluded(const SyncPath &) override { return false; }
        bool fileStatusChanged(const SyncPath &, const SyncFileStatus) override { return true; }

        void clearFileAttributes(const SyncPath &) override { /*VfsOff*/ }
        void dehydrate(const SyncPath &) override { /*VfsOff*/ }
        void hydrate(const SyncPath &) override { /*VfsOff*/ }
        void cancelHydrate(const SyncPath &) override { /*VfsOff*/ }

    protected:
        ExitInfo startImpl(bool &installationDone, bool &activationDone, bool &connectionDone) override;
        void stopImpl(bool /*unregister*/) override { /*VfsOff*/ }

        friend class TestWorkers;
};

/// Check whether the plugin for the mode is available.
bool isVfsPluginAvailable(VirtualFileMode virtualFileMode, QString &error);

/// Return the best available VFS mode.
VirtualFileMode bestAvailableVfsMode();

/// Create a VFS instance for the mode, returns nullptr on failure.
std::unique_ptr<Vfs> createVfsFromPlugin(VirtualFileMode virtualFileMode, VfsSetupParams &vfsSetupParams, QString &error);

} // namespace KDC
