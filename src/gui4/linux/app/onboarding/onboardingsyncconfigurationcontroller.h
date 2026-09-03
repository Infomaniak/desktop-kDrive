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
#include "app/onboarding/selectedsyncconfigurationsmodel.h"
#include "app/syncconfiguration/remotefolderprovider.h"
#include "app/syncconfiguration/remotefoldertreemodel.h"

#include <QObject>
#include <QUrl>

#include <optional>
#include <vector>

namespace KDC {

class AppCache;
class CommService;
class OnboardingFlowController;
class OnboardingState;

/** Session-scoped transactional editor for onboarding synchronization settings. */
class OnboardingSyncConfigurationController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
        Q_PROPERTY(Page page READ page NOTIFY presentationChanged)
        Q_PROPERTY(bool driveConfigurationPage READ driveConfigurationPage NOTIFY presentationChanged)
        Q_PROPERTY(bool folderSelectionPage READ folderSelectionPage NOTIFY presentationChanged)
        Q_PROPERTY(bool busy READ busy NOTIFY presentationChanged)
        Q_PROPERTY(bool canValidate READ canValidate NOTIFY presentationChanged)
        Q_PROPERTY(QString errorTitle READ errorTitle NOTIFY presentationChanged)
        Q_PROPERTY(QString errorText READ errorText NOTIFY presentationChanged)
        Q_PROPERTY(QString currentDriveName READ currentDriveName NOTIFY presentationChanged)
        Q_PROPERTY(QColor currentDriveColor READ currentDriveColor NOTIFY presentationChanged)
        Q_PROPERTY(QString currentLocalPath READ currentLocalPath NOTIFY presentationChanged)
        Q_PROPERTY(bool currentUsesDefaultFolder READ currentUsesDefaultFolder NOTIFY presentationChanged)
        Q_PROPERTY(bool currentHasCustomSelection READ currentHasCustomSelection NOTIFY presentationChanged)
        Q_PROPERTY(SelectedSyncConfigurationsModel *selectedDrivesModel READ selectedDrivesModel CONSTANT)
        Q_PROPERTY(RemoteFolderTreeModel *folderTreeModel READ folderTreeModel CONSTANT)

    public:
        enum Page : uint8_t {
            Summary,
            DriveConfiguration,
            FolderSelection,
        };
        Q_ENUM(Page)

        explicit OnboardingSyncConfigurationController(AppCache &appCache, OnboardingState &onboardingState,
                                                       OnboardingFlowController &flowController, CommService &commService,
                                                       QObject *parent = nullptr);

        [[nodiscard]] bool visible() const { return _visible; }
        [[nodiscard]] Page page() const { return _page; }
        [[nodiscard]] bool driveConfigurationPage() const { return _page == DriveConfiguration; }
        [[nodiscard]] bool folderSelectionPage() const { return _page == FolderSelection; }
        [[nodiscard]] bool busy() const { return _busy; }
        [[nodiscard]] bool canValidate() const;
        [[nodiscard]] QString errorTitle() const { return _errorTitle; }
        [[nodiscard]] QString errorText() const { return _errorText; }
        [[nodiscard]] QString currentDriveName() const;
        [[nodiscard]] QColor currentDriveColor() const;
        /** Local folder of the drive being configured, in its `~`-shortened display form. */
        [[nodiscard]] QString currentLocalPath() const;
        [[nodiscard]] bool currentUsesDefaultFolder() const;
        [[nodiscard]] bool currentHasCustomSelection() const;
        [[nodiscard]] SelectedSyncConfigurationsModel *selectedDrivesModel() { return &_selectedDrivesModel; }
        [[nodiscard]] RemoteFolderTreeModel *folderTreeModel() { return &_folderTreeModel; }

        Q_INVOKABLE void open();
        Q_INVOKABLE void configureDrive(int32_t row);
        Q_INVOKABLE void cancelCurrentPage();
        Q_INVOKABLE void validateCurrentPage();
        Q_INVOKABLE void requestCustomFolder();
        /** Reports that the native folder picker closed, whatever the outcome, so focus can return to its button. */
        Q_INVOKABLE void notifyCustomFolderDialogClosed();
        Q_INVOKABLE void applyCustomFolder(const QUrl &folderUrl);
        Q_INVOKABLE void returnToDefaultFolder();
        Q_INVOKABLE void selectFolders();

    signals:
        void visibleChanged();
        void presentationChanged();
        void customFolderRequested(const QUrl &initialFolder);
        void customFolderDialogClosed();

    private:
        struct Draft {
                AvailableDriveKey key;
                QString driveName;
                QColor driveColor;
                PendingSyncConfig config;
        };

        [[nodiscard]] Draft *currentDraft();
        [[nodiscard]] const Draft *currentDraft() const;
        [[nodiscard]] bool conflictsWithAnotherDraft(const QString &path, int32_t excludedRow) const;
        void buildDrafts();
        void showInitialPage();
        void openDrive(int32_t row);
        void refreshSummaryModel();
        void setBusy(bool busy);
        void abortPendingRequest();
        void clearError();
        void setError(const QString &title, const QString &text);
        void closeWithoutCommit();
        void commitAndClose();

        AppCache &_appCache;
        OnboardingState &_onboardingState;
        OnboardingFlowController &_flowController;
        CommService &_commService;
        SelectedSyncConfigurationsModel _selectedDrivesModel;
        CommRemoteFolderProvider _folderProvider;
        RemoteFolderTreeModel _folderTreeModel;
        std::vector<Draft> _drafts;
        std::optional<PendingSyncConfig> _driveSnapshot;
        int32_t _currentRow{-1};
        Page _page{Summary};
        bool _visible{false};
        bool _busy{false};
        QString _errorTitle;
        QString _errorText;
        uint64_t _requestGeneration{0};
};

} // namespace KDC
