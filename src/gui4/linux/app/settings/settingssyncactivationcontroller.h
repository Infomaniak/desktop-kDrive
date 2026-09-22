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

#include "app/cache/cachetypes.h"
#include "app/syncconfiguration/remotefolderprovider.h"
#include "app/syncconfiguration/remotefoldertreemodel.h"

#include <QColor>
#include <QObject>
#include <QString>
#include <QUrl>

#include <cstdint>
#include <unordered_set>

namespace KDC {

class AppCache;
class CachePopulator;
class CommService;
class ServiceEventBus;
struct GoodPathResult;

/** Transactional editor used to activate one available drive from Settings. */
class SettingsSyncActivationController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
        Q_PROPERTY(bool driveConfigurationPage READ driveConfigurationPage NOTIFY pageChanged)
        Q_PROPERTY(bool folderSelectionPage READ folderSelectionPage NOTIFY pageChanged)
        Q_PROPERTY(bool preparing READ preparing NOTIFY presentationChanged)
        Q_PROPERTY(bool busy READ busy NOTIFY presentationChanged)
        Q_PROPERTY(bool canValidate READ canValidate NOTIFY presentationChanged)
        Q_PROPERTY(QString localFolderErrorText READ localFolderErrorText NOTIFY presentationChanged)
        Q_PROPERTY(QString operationErrorText READ operationErrorText NOTIFY presentationChanged)
        Q_PROPERTY(QString currentDriveName READ currentDriveName NOTIFY presentationChanged)
        Q_PROPERTY(QColor currentDriveColor READ currentDriveColor NOTIFY presentationChanged)
        Q_PROPERTY(QString currentLocalPath READ currentLocalPath NOTIFY presentationChanged)
        Q_PROPERTY(bool currentUsesDefaultFolder READ currentUsesDefaultFolder NOTIFY presentationChanged)
        Q_PROPERTY(bool currentHasCustomSelection READ currentHasCustomSelection NOTIFY presentationChanged)
        Q_PROPERTY(RemoteFolderTreeModel *folderTreeModel READ folderTreeModel CONSTANT)

    public:
        explicit SettingsSyncActivationController(AppCache &appCache, CommService &commService, CachePopulator &cachePopulator,
                                                  ServiceEventBus &serviceEventBus, QObject *parent = nullptr);

        [[nodiscard]] bool visible() const { return _visible; }
        [[nodiscard]] bool driveConfigurationPage() const { return _page == Page::DriveConfiguration; }
        [[nodiscard]] bool folderSelectionPage() const { return _page == Page::FolderSelection; }
        [[nodiscard]] bool preparing() const { return _preparing; }
        [[nodiscard]] bool busy() const { return _busy; }
        [[nodiscard]] bool canValidate() const;
        [[nodiscard]] QString localFolderErrorText() const;
        [[nodiscard]] QString operationErrorText() const;
        [[nodiscard]] QString currentDriveName() const { return _driveName; }
        [[nodiscard]] QColor currentDriveColor() const { return _driveColor; }
        [[nodiscard]] QString currentLocalPath() const;
        [[nodiscard]] bool currentUsesDefaultFolder() const { return _config.usesDefaultLocalPath; }
        [[nodiscard]] bool currentHasCustomSelection() const { return !_config.blackList.empty(); }
        [[nodiscard]] RemoteFolderTreeModel *folderTreeModel() { return &_folderTreeModel; }

        Q_INVOKABLE void activate(qint64 userDbId, qint64 accountId, qint64 driveId);
        Q_INVOKABLE [[nodiscard]] bool targets(qint64 userDbId, qint64 accountId, qint64 driveId) const;
        Q_INVOKABLE void cancelCurrentPage();
        /// Dismisses the editor when its host window closes without abandoning an already submitted synchronization.
        Q_INVOKABLE void dismissFromHostWindow();
        Q_INVOKABLE void validateCurrentPage();
        Q_INVOKABLE void requestCustomFolder();
        Q_INVOKABLE void notifyCustomFolderDialogClosed();
        Q_INVOKABLE void applyCustomFolder(const QUrl &folderUrl);
        Q_INVOKABLE void returnToDefaultFolder();
        Q_INVOKABLE void selectFolders();
        void retranslate();

    signals:
        void visibleChanged();
        void pageChanged();
        void presentationChanged();
        void customFolderRequested(const QUrl &initialFolder);
        void customFolderDialogClosed();

    private:
        enum class Page : uint8_t {
            DriveConfiguration,
            FolderSelection,
        };

        [[nodiscard]] bool targetStillAvailable() const;
        void requestDefaultFolder();
        void handleDefaultFolderProposal(uint64_t generation, const ExitInfo &exitInfo, const GoodPathResult &result);
        void createSynchronization();
        void handleReconciliationFinished(bool succeeded);
        void handleTargetStateChanged();
        void setPage(Page page);
        void setPreparing(bool preparing);
        void setBusy(bool busy);
        void setLocalFolderErrorId(const QString &translationId);
        void setOperationErrorId(const QString &translationId);
        void close();
        void resetTarget();

        AppCache &_appCache;
        CommService &_commService;
        CachePopulator &_cachePopulator;
        ServiceEventBus &_serviceEventBus;
        CommRemoteFolderProvider _folderProvider;
        RemoteFolderTreeModel _folderTreeModel;
        AvailableDriveKey _key;
        PendingSyncConfig _config;
        QString _driveName;
        QColor _driveColor;
        QString _localFolderErrorId;
        QString _operationErrorId;
        Page _page{Page::DriveConfiguration};
        uint64_t _requestGeneration{0};
        std::unordered_set<AvailableDriveKey> _reconciliationBlockedKeys;
        bool _visible{false};
        bool _preparing{false};
        bool _busy{false};
        bool _reconciliationPending{false};
        bool _reconciliationForActivation{false};
        bool _syncCreationPending{false};
};

} // namespace KDC
