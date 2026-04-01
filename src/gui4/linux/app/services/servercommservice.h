/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "communicationlayer/signaldispatcher.h"
#include "libcommon/utility/cstypes.h"
#include "libcommon/utility/types.h"
#include "libcommon/info/accountinfo.h"
#include "libcommon/info/driveinfo.h"
#include "libcommon/info/errorinfo.h"
#include "libcommon/info/syncfileiteminfo.h"
#include "libcommon/info/syncinfo.h"
#include "libcommon/info/userinfo.h"

#include <QObject>

namespace KDC {

/**
 * Registers handlers on the SignalDispatcher for every server-initiated signal
 * and re-emits them as typed Qt signals for consumption by app models and QML.
 */
class ServerCommService : public QObject {
        Q_OBJECT

    public:
        explicit ServerCommService(SignalDispatcher &dispatcher, QObject *parent = nullptr);

    signals:
        // --- User ---
        void userAdded(const UserInfo &info);
        void userUpdated(const UserInfo &info);
        void userRemoved(UserDbId userDbId);

        // --- Account ---
        void accountAdded(const AccountInfo &info);
        void accountUpdated(const AccountInfo &info);
        void accountRemoved(AccountDbId accountDbId);

        // --- Drive ---
        void driveAdded(const DriveInfo &info);
        void driveUpdated(const DriveInfo &info);
        void driveRemoved(DriveDbId driveDbId);

        // --- Sync ---
        void syncAdded(const SyncInfo &info);
        void syncUpdated(const SyncInfo &info);
        void syncRemoved(SyncDbId syncDbId);
        void syncProgressInfo(SyncDbId syncDbId, SyncStatus status, SyncStep step, int64_t currentFile, int64_t totalFiles,
                              int64_t completedSize, int64_t totalSize, int64_t estimatedRemainingTime);
        void itemCompleted(SyncDbId syncDbId, const SyncFileItemInfo &info);

        // --- Error ---
        void errorAdded(const ErrorInfo &info);
        void errorRemoved(ErrorDbId errorDbId);

        // --- Updater ---
        void updateStateChanged(UpdateState state);
        void showUpdateDialog(const VersionInfo &info);

        // --- Utility ---
        void showNotification(const QString &title, const QString &message);
        void showSettings();
        void showSynthesis();
        void logUploadStatusUpdated(LogUploadState state, int32_t percentage);
        void quit();

    private:
        void registerUserHandlers(SignalDispatcher &dispatcher);
        void registerAccountHandlers(SignalDispatcher &dispatcher);
        void registerDriveHandlers(SignalDispatcher &dispatcher);
        void registerSyncHandlers(SignalDispatcher &dispatcher);
        void registerErrorHandlers(SignalDispatcher &dispatcher);
        void registerUpdaterHandlers(SignalDispatcher &dispatcher);
        void registerUtilityHandlers(SignalDispatcher &dispatcher);
};

} // namespace KDC
