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

#include <QColor>
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
 * destination chosen in `RemoteFolderPickerModel`, where the user can also create a folder. Only the final submission
 * sends SYNC_ADD; the dialog then closes and the new synchronization reaches the advanced sync page through the
 * SYNC_ADDED push. A remote folder created meanwhile stays on kDrive if the dialog is cancelled.
 */
class AdvancedSyncCreationController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
        Q_PROPERTY(bool locationPickerOpen READ locationPickerOpen NOTIFY presentationChanged)
        Q_PROPERTY(bool busy READ busy NOTIFY presentationChanged)
        Q_PROPERTY(bool checkingLocalFolder READ checkingLocalFolder NOTIFY presentationChanged)
        Q_PROPERTY(bool submitting READ submitting NOTIFY presentationChanged)
        Q_PROPERTY(bool hasLocalFolder READ hasLocalFolder NOTIFY presentationChanged)
        Q_PROPERTY(QString localFolderName READ localFolderName NOTIFY presentationChanged)
        Q_PROPERTY(QString localPath READ localPath NOTIFY presentationChanged)
        Q_PROPERTY(bool localFolderInvalid READ localFolderInvalid NOTIFY presentationChanged)
        Q_PROPERTY(bool hasRemoteFolder READ hasRemoteFolder NOTIFY presentationChanged)
        Q_PROPERTY(QString remoteFolderName READ remoteFolderName NOTIFY presentationChanged)
        Q_PROPERTY(QString remotePath READ remotePath NOTIFY presentationChanged)
        Q_PROPERTY(bool canSubmit READ canSubmit NOTIFY presentationChanged)
        Q_PROPERTY(bool submitFailed READ submitFailed NOTIFY presentationChanged)
        Q_PROPERTY(bool canConfirmLocation READ canConfirmLocation NOTIFY presentationChanged)
        Q_PROPERTY(bool folderCreationFailed READ folderCreationFailed NOTIFY presentationChanged)
        Q_PROPERTY(QColor driveColor READ driveColor NOTIFY presentationChanged)
        Q_PROPERTY(RemoteFolderPickerModel *pickerModel READ pickerModel CONSTANT)

    public:
        AdvancedSyncCreationController(AppCache &appCache, CommService &commService, SyncService &syncService,
                                       QObject *parent = nullptr);

        [[nodiscard]] bool visible() const { return _state != State::Closed; }
        [[nodiscard]] bool locationPickerOpen() const { return _locationPickerOpen; }
        // A request the user cannot interrupt is running: the dialog cannot be cancelled meanwhile.
        [[nodiscard]] bool busy() const;
        [[nodiscard]] bool checkingLocalFolder() const { return _state == State::CheckingLocalFolder; }
        [[nodiscard]] bool submitting() const { return _state == State::Submitting; }
        [[nodiscard]] bool hasLocalFolder() const { return !_localPath.isEmpty(); }
        [[nodiscard]] QString localFolderName() const;
        [[nodiscard]] QString localPath() const;
        [[nodiscard]] bool localFolderInvalid() const { return _localFolderInvalid; }
        [[nodiscard]] bool hasRemoteFolder() const { return !_remoteNodeId.isEmpty(); }
        [[nodiscard]] QString remoteFolderName() const { return _remoteFolderName; }
        [[nodiscard]] QString remotePath() const { return _remotePath; }
        [[nodiscard]] bool canSubmit() const;
        [[nodiscard]] bool submitFailed() const { return _submitFailed; }
        [[nodiscard]] bool canConfirmLocation() const;
        // The typed name was refused, locally for a path separator or by the server.
        [[nodiscard]] bool folderCreationFailed() const { return _folderCreationFailed; }
        [[nodiscard]] QColor driveColor() const { return _driveColor; }
        [[nodiscard]] RemoteFolderPickerModel *pickerModel() { return &_pickerModel; }

        Q_INVOKABLE void open(qint64 driveDbId);
        /// Leaves the location picker for the form, or closes the dialog from the form.
        Q_INVOKABLE void cancelCurrentPage();
        /// Closes the dialog when its host window closes; a submitted synchronization is not abandoned.
        Q_INVOKABLE void dismissFromHostWindow();

        Q_INVOKABLE void requestLocalFolder();
        Q_INVOKABLE void notifyLocalFolderDialogClosed();
        Q_INVOKABLE void applyLocalFolder(const QUrl &folderUrl);

        Q_INVOKABLE void openLocationPicker();
        Q_INVOKABLE void confirmLocation();
        Q_INVOKABLE void beginFolderCreation(const QModelIndex &parentIndex);
        /// Creates the folder named by the editing row. An empty name cancels the creation.
        Q_INVOKABLE void commitFolderCreation(const QString &name);
        Q_INVOKABLE void cancelFolderCreation();

        Q_INVOKABLE void submit();

    signals:
        void visibleChanged();
        void presentationChanged();
        void localFolderRequested(const QUrl &initialFolder);
        void localFolderDialogClosed();

    private:
        enum class State : uint8_t {
            Closed, // No target.
            Editing, // Waiting for the user.
            CheckingLocalFolder, // Local folder validation in flight.
            Submitting, // SYNC_ADD in flight.
        };

        [[nodiscard]] bool targetStillExists() const;
        void handleTargetStateChanged();
        void handleCreatedFolder(const NodeId &nodeId, const QString &name, const QString &parentNodeId,
                                 const QString &parentPath);
        void setState(State state);
        void setFolderCreationFailed(bool failed);
        void close();

        AppCache &_appCache;
        CommService &_commService;
        SyncService &_syncService;
        CommRemoteFolderProvider _folderProvider;
        RemoteFolderPickerModel _pickerModel;
        DriveDbId _driveDbId{0};
        UserDbId _userDbId{0};
        AccountId _accountId{0};
        DriveId _driveId{0};
        QString _driveName;
        QColor _driveColor;
        QString _localPath;
        QString _remoteNodeId;
        QString _remoteFolderName;
        QString _remotePath;
        State _state{State::Closed};
        // Invalidates every response of a previous target, folder creations included.
        uint64_t _targetGeneration{0};
        // Invalidates the response of a superseded local folder validation or submission.
        uint64_t _requestGeneration{0};
        bool _locationPickerOpen{false};
        // The picker tree is loaded on its first opening, then kept for the lifetime of the dialog.
        bool _pickerConfigured{false};
        bool _localFolderInvalid{false};
        bool _submitFailed{false};
        bool _folderCreationFailed{false};
};

} // namespace KDC
