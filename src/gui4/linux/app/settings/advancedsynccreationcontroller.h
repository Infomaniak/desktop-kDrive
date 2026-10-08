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

#include "app/syncconfiguration/remotefolderpickermodel.h"
#include "app/syncconfiguration/remotefolderprovider.h"
#include "libcommon/utility/types.h"

#include <QObject>
#include <QString>
#include <QUrl>

#include <cstdint>

namespace KDC {

class AppCache;
class CommService;
class SyncService;

/**
 * Transactional editor of the Settings "Sync a folder with kDrive" dialog, which creates one advanced synchronization.
 *
 * Role: keep the draft of a local folder, validated by the server as an advanced synchronization folder, and of a remote
 * destination chosen in `RemoteFolderPickerModel`. Only the final submission sends SYNC_ADD; the dialog then closes and
 * the new synchronization reaches the advanced sync page through the SYNC_ADDED push.
 *
 * Lifetime: one instance per dialog session, created and released by `AdvancedSyncsController`. A response received
 * after the release reaches a destroyed instance and is dropped.
 */
class AdvancedSyncCreationController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool active READ active NOTIFY activeChanged)
        Q_PROPERTY(bool locationPickerOpen READ locationPickerOpen NOTIFY presentationChanged)
        Q_PROPERTY(bool busy READ busy NOTIFY presentationChanged)
        Q_PROPERTY(bool checkingLocalFolder READ checkingLocalFolder NOTIFY presentationChanged)
        Q_PROPERTY(bool submitting READ submitting NOTIFY presentationChanged)
        Q_PROPERTY(QString localFolderName READ localFolderName NOTIFY presentationChanged)
        Q_PROPERTY(QString localPath READ localPath NOTIFY presentationChanged)
        Q_PROPERTY(bool localFolderInvalid READ localFolderInvalid NOTIFY presentationChanged)
        Q_PROPERTY(QString remoteFolderName READ remoteFolderName NOTIFY presentationChanged)
        Q_PROPERTY(QString remotePath READ remotePath NOTIFY presentationChanged)
        Q_PROPERTY(bool canSubmit READ canSubmit NOTIFY presentationChanged)
        Q_PROPERTY(bool submitFailed READ submitFailed NOTIFY presentationChanged)
        Q_PROPERTY(bool canConfirmLocation READ canConfirmLocation NOTIFY presentationChanged)
        Q_PROPERTY(RemoteFolderPickerModel *pickerModel READ pickerModel CONSTANT)

    public:
        AdvancedSyncCreationController(AppCache &appCache, CommService &commService, SyncService &syncService,
                                       DriveDbId driveDbId, QObject *parent = nullptr);

        // False once the session is over: the dialog closes, then its host releases this instance.
        [[nodiscard]] bool active() const { return _state != State::Finished; }
        [[nodiscard]] bool locationPickerOpen() const { return _locationPickerOpen; }
        // A request the user cannot interrupt is running: the dialog cannot be cancelled meanwhile.
        [[nodiscard]] bool busy() const;
        [[nodiscard]] bool checkingLocalFolder() const { return _state == State::CheckingLocalFolder; }
        [[nodiscard]] bool submitting() const { return _state == State::Submitting; }
        [[nodiscard]] QString localFolderName() const;
        [[nodiscard]] QString localPath() const;
        [[nodiscard]] bool localFolderInvalid() const { return _localFolderInvalid; }
        [[nodiscard]] QString remoteFolderName() const { return _remoteFolderName; }
        [[nodiscard]] QString remotePath() const { return _remotePath; }
        [[nodiscard]] bool canSubmit() const;
        [[nodiscard]] bool submitFailed() const { return _submitFailed; }
        [[nodiscard]] bool canConfirmLocation() const;
        [[nodiscard]] RemoteFolderPickerModel *pickerModel() { return &_pickerModel; }

        /// Leaves the location picker for the form, or ends the session from the form.
        Q_INVOKABLE void cancelCurrentPage();

        Q_INVOKABLE void requestLocalFolder();
        Q_INVOKABLE void notifyLocalFolderDialogClosed();
        Q_INVOKABLE void applyLocalFolder(const QUrl &folderUrl);

        Q_INVOKABLE void openLocationPicker();
        Q_INVOKABLE void confirmLocation();

        Q_INVOKABLE void submit();

    signals:
        void activeChanged();
        void presentationChanged();
        void localFolderRequested(const QUrl &initialFolder);
        void localFolderDialogClosed();

    private:
        enum class State : uint8_t {
            Editing, // Waiting for the user.
            CheckingLocalFolder, // Local folder validation in flight.
            Submitting, // SYNC_ADD in flight.
            Finished, // Cancelled or created, waiting for its release.
        };

        void setState(State state);
        void finish();

        AppCache &_appCache;
        CommService &_commService;
        SyncService &_syncService;
        CommRemoteFolderProvider _folderProvider;
        RemoteFolderPickerModel _pickerModel;
        const DriveDbId _driveDbId;
        QString _localPath;
        QString _remoteNodeId;
        QString _remoteFolderName;
        QString _remotePath;
        State _state{State::Editing};
        bool _locationPickerOpen{false};
        // The picker tree is loaded on its first opening, then kept for the session.
        bool _pickerConfigured{false};
        bool _localFolderInvalid{false};
        bool _submitFailed{false};
};

} // namespace KDC
