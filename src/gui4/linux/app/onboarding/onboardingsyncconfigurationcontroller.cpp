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

#include "onboardingsyncconfigurationcontroller.h"

#include "app/appconstants.h"
#include "app/cache/appcache.h"
#include "app/cache/onboardingstate.h"
#include "app/onboarding/onboardingflowcontroller.h"
#include "app/services/commservice.h"
#include "app/syncconfiguration/localpaths.h"
#include "libcommon/utility/utility.h"

#include <QDir>
#include <QFileInfo>
#include <QPointer>

#include <algorithm>
#include <unordered_map>

using namespace Qt::StringLiterals;

namespace KDC {

OnboardingSyncConfigurationController::OnboardingSyncConfigurationController(AppCache &appCache, OnboardingState &onboardingState,
                                                                             OnboardingFlowController &flowController,
                                                                             CommService &commService, QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _onboardingState(onboardingState),
    _flowController(flowController),
    _commService(commService),
    _selectedDrivesModel(this),
    _folderProvider(commService),
    _folderTreeModel(_folderProvider, this) {
    (void) connect(&_flowController, &OnboardingFlowController::advancedSettingsRequested, this,
                   &OnboardingSyncConfigurationController::open);
    (void) connect(&_folderTreeModel, &RemoteFolderTreeModel::stateChanged, this,
                   &OnboardingSyncConfigurationController::presentationChanged);
}

bool OnboardingSyncConfigurationController::canValidate() const {
    if (_busy) return false;
    if (_page == FolderSelection) return !_folderTreeModel.loading() && !_folderTreeModel.loadFailed();
    if (_page == DriveConfiguration) return currentDraft() && !currentDraft()->config.localPath.isEmpty();
    return !_drafts.empty() && std::ranges::all_of(_drafts, [](const Draft &draft) { return !draft.config.localPath.isEmpty(); });
}

QString OnboardingSyncConfigurationController::currentDriveName() const {
    return currentDraft() ? currentDraft()->driveName : QString{};
}

QColor OnboardingSyncConfigurationController::currentDriveColor() const {
    return currentDraft() ? currentDraft()->driveColor : AppConstants::Drive::defaultColor();
}

QString OnboardingSyncConfigurationController::currentLocalPath() const {
    return currentDraft() ? displayLocalPath(currentDraft()->config.localPath) : QString{};
}

bool OnboardingSyncConfigurationController::currentUsesDefaultFolder() const {
    return currentDraft() && currentDraft()->config.usesDefaultLocalPath;
}

bool OnboardingSyncConfigurationController::currentHasCustomSelection() const {
    return currentDraft() && !currentDraft()->config.blackList.empty();
}

void OnboardingSyncConfigurationController::open() {
    if (_visible) return;
    buildDrafts();
    if (_drafts.empty()) return;

    ++_requestGeneration;
    _visible = true;
    _busy = false;
    _errorTitle.clear();
    _errorText.clear();
    _page = Summary;
    _currentRow = -1;
    emit visibleChanged();
    showInitialPage();
}

void OnboardingSyncConfigurationController::configureDrive(const int32_t row) {
    if (_busy || row < 0 || static_cast<std::size_t>(row) >= _drafts.size()) return;
    openDrive(row);
}

void OnboardingSyncConfigurationController::cancelCurrentPage() {
    // Leaving the page drops the path validation it started: its result would land on a page the user has left.
    abortPendingRequest();
    clearError();
    if (_page == FolderSelection) {
        _page = DriveConfiguration;
        emit presentationChanged();
        return;
    }
    if (_page == DriveConfiguration) {
        if (_driveSnapshot && currentDraft()) currentDraft()->config = *_driveSnapshot;
        refreshSummaryModel();
        if (_drafts.size() == 1) {
            closeWithoutCommit();
        } else {
            _page = Summary;
            _currentRow = -1;
            emit presentationChanged();
        }
        return;
    }
    closeWithoutCommit();
}

void OnboardingSyncConfigurationController::validateCurrentPage() {
    clearError();
    if (!canValidate()) return;
    if (_page == FolderSelection) {
        if (Draft *const draft = currentDraft()) draft->config.blackList = _folderTreeModel.blackList();
        _page = DriveConfiguration;
        refreshSummaryModel();
        emit presentationChanged();
        return;
    }
    if (_page == DriveConfiguration) {
        refreshSummaryModel();
        if (_drafts.size() == 1) {
            commitAndClose();
        } else {
            _page = Summary;
            _currentRow = -1;
            emit presentationChanged();
        }
        return;
    }
    commitAndClose();
}

void OnboardingSyncConfigurationController::requestCustomFolder() {
    if (_busy || !currentDraft()) return;
    const QFileInfo currentLocation(currentDraft()->config.localPath);
    emit customFolderRequested(
            QUrl::fromLocalFile(currentLocation.exists() ? currentLocation.absoluteFilePath() : currentLocation.absolutePath()));
}

void OnboardingSyncConfigurationController::notifyCustomFolderDialogClosed() {
    emit customFolderDialogClosed();
}

void OnboardingSyncConfigurationController::applyCustomFolder(const QUrl &folderUrl) {
    if (_busy) return;
    const Draft *const draft = currentDraft();
    const QString path = QDir::cleanPath(folderUrl.toLocalFile());
    if (!draft || path.isEmpty()) return;
    if (conflictsWithAnotherDraft(path, _currentRow)) {
        setError(qtTrId("teachingTipInvalidFolderTitle"), qtTrId("teachingTipInvalidFolderContent"));
        return;
    }

    setBusy(true);
    clearError();
    const uint64_t generation = ++_requestGeneration;
    const QPointer self(this);
    _commService.requestIsPathValidForNewSync(
            QStr2Path(path), SyncConfiguration::Classic, [self, generation, path](const ExitInfo &exitInfo, const bool valid) {
                if (!self || generation != self->_requestGeneration) return;
                self->setBusy(false);
                if (!exitInfo || !valid || !self->currentDraft()) {
                    self->setError(qtTrId("teachingTipInvalidFolderTitle"), qtTrId("teachingTipInvalidFolderContent"));
                    return;
                }
                self->currentDraft()->config.localPath = path;
                self->currentDraft()->config.usesDefaultLocalPath =
                        QDir::cleanPath(self->currentDraft()->config.defaultLocalPath) == path;
                self->refreshSummaryModel();
                emit self->presentationChanged();
            });
}

void OnboardingSyncConfigurationController::returnToDefaultFolder() {
    if (_busy) return;
    Draft *const draft = currentDraft();
    if (!draft || draft->config.defaultLocalPath.isEmpty()) return;
    // Another drive may have taken that folder while this one sat on a custom path.
    if (conflictsWithAnotherDraft(draft->config.defaultLocalPath, _currentRow)) {
        setError(qtTrId("teachingTipInvalidFolderTitle"), qtTrId("teachingTipInvalidFolderContent"));
        return;
    }
    draft->config.localPath = draft->config.defaultLocalPath;
    draft->config.usesDefaultLocalPath = true;
    clearError();
    refreshSummaryModel();
    emit presentationChanged();
}

void OnboardingSyncConfigurationController::selectFolders() {
    const Draft *const draft = currentDraft();
    if (!draft || _busy) return;
    _folderTreeModel.configure(draft->key.userDbId, draft->key.driveId, QStr2Str(draft->config.targetNodeId),
                               draft->config.blackList);
    _page = FolderSelection;
    clearError();
    emit presentationChanged();
}

OnboardingSyncConfigurationController::Draft *OnboardingSyncConfigurationController::currentDraft() {
    return _currentRow >= 0 && static_cast<std::size_t>(_currentRow) < _drafts.size()
                   ? &_drafts[static_cast<std::size_t>(_currentRow)]
                   : nullptr;
}

const OnboardingSyncConfigurationController::Draft *OnboardingSyncConfigurationController::currentDraft() const {
    return _currentRow >= 0 && static_cast<std::size_t>(_currentRow) < _drafts.size()
                   ? &_drafts[static_cast<std::size_t>(_currentRow)]
                   : nullptr;
}

bool OnboardingSyncConfigurationController::conflictsWithAnotherDraft(const QString &path, const int32_t excludedRow) const {
    for (int32_t row = 0; static_cast<std::size_t>(row) < _drafts.size(); ++row) {
        if (row == excludedRow || _drafts[static_cast<std::size_t>(row)].config.localPath.isEmpty()) continue;
        if (localPathsOverlap(path, _drafts[static_cast<std::size_t>(row)].config.localPath)) return true;
    }
    return false;
}

void OnboardingSyncConfigurationController::buildDrafts() {
    // Reconciles the drafts with the selection instead of reloading them: a rejected commit rebuilds them with the
    // modal still open, and what the user has just configured has to survive that retry.
    std::unordered_map<AvailableDriveKey, PendingSyncConfig> editedConfigs;
    editedConfigs.reserve(_drafts.size());
    for (const auto &draft: _drafts) editedConfigs.emplace(draft.key, draft.config);

    _drafts.clear();
    for (const auto &key: _onboardingState.selectedAvailableDriveKeys()) {
        const auto availableDrive = _appCache.availableDrive(key);
        if (!availableDrive) continue;
        const auto editedConfig = editedConfigs.find(key);
        PendingSyncConfig config = editedConfig != editedConfigs.cend()
                                           ? editedConfig->second
                                           : _onboardingState.pendingSyncConfig(key).value_or(PendingSyncConfig{});
        if (!config.localPath.isEmpty() && config.defaultLocalPath.isEmpty()) config.defaultLocalPath = config.localPath;
        _drafts.push_back({.key = key,
                           .driveName = QString::fromStdString(availableDrive->name()),
                           .driveColor = QColor(QString::fromStdString(availableDrive->color())),
                           .config = config});
    }
    std::ranges::sort(_drafts, [](const Draft &lhs, const Draft &rhs) {
        if (const int nameComparison = QString::compare(lhs.driveName, rhs.driveName, Qt::CaseInsensitive); nameComparison != 0) {
            return nameComparison < 0;
        }
        if (lhs.key.accountId != rhs.key.accountId) {
            return lhs.key.accountId < rhs.key.accountId;
        }
        return lhs.key.driveId < rhs.key.driveId;
    });
    refreshSummaryModel();
}

void OnboardingSyncConfigurationController::showInitialPage() {
    if (_drafts.size() == 1) {
        openDrive(0);
    } else {
        _page = Summary;
        _currentRow = -1;
        emit presentationChanged();
    }
}

void OnboardingSyncConfigurationController::openDrive(const int32_t row) {
    _currentRow = row;
    _driveSnapshot = _drafts[static_cast<std::size_t>(row)].config;
    _page = DriveConfiguration;
    clearError();
    emit presentationChanged();
}

void OnboardingSyncConfigurationController::refreshSummaryModel() {
    std::vector<SelectedSyncConfigurationRow> rows;
    rows.reserve(_drafts.size());
    for (const auto &draft: _drafts) {
        rows.push_back({.driveName = draft.driveName,
                        .driveColor = draft.driveColor.isValid() ? draft.driveColor : AppConstants::Drive::defaultColor(),
                        .localPath = displayLocalPath(draft.config.localPath),
                        .customFolder = !draft.config.usesDefaultLocalPath,
                        .customSelection = !draft.config.blackList.empty()});
    }
    _selectedDrivesModel.setRows(std::move(rows));
}

void OnboardingSyncConfigurationController::setBusy(const bool busy) {
    if (_busy == busy) return;
    _busy = busy;
    emit presentationChanged();
}

void OnboardingSyncConfigurationController::abortPendingRequest() {
    ++_requestGeneration;
    setBusy(false);
}

void OnboardingSyncConfigurationController::clearError() {
    setError({}, {});
}

void OnboardingSyncConfigurationController::setError(const QString &title, const QString &text) {
    if (_errorTitle == title && _errorText == text) return;
    _errorTitle = title;
    _errorText = text;
    emit presentationChanged();
}

void OnboardingSyncConfigurationController::closeWithoutCommit() {
    if (!_visible) return;
    ++_requestGeneration;
    _visible = false;
    _busy = false;
    _drafts.clear();
    _driveSnapshot.reset();
    _currentRow = -1;
    _page = Summary;
    _errorTitle.clear();
    _errorText.clear();
    emit visibleChanged();
    emit presentationChanged();
}

void OnboardingSyncConfigurationController::commitAndClose() {
    std::unordered_map<AvailableDriveKey, PendingSyncConfig> configs;
    configs.reserve(_drafts.size());
    for (const auto &draft: _drafts) configs.emplace(draft.key, draft.config);
    if (!_onboardingState.replacePendingSyncConfigs(configs)) {
        // The selected drives changed under the modal, so the drafts no longer describe them. Closing here would
        // silently drop everything the user configured.
        setError({}, qtTrId("onboardingLoginErrorDescription"));
        buildDrafts();
        if (_drafts.empty()) {
            closeWithoutCommit();
            return;
        }
        _page = Summary;
        _currentRow = -1;
        emit presentationChanged();
        return;
    }
    closeWithoutCommit();
}

} // namespace KDC
