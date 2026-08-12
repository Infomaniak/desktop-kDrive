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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "app/mainwindow/activitiescontroller.h"

#include "libcommon/utility/types.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QUrl>

#include <algorithm>
#include <cstdint>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcActivitiesController, "gui.v4.activitiescontroller", QtInfoMsg)

enum class ActivitiesTitleState : uint8_t {
    Loading,
    Offline,
    InProgress,
    NoActivity,
    Idle,
    Paused,
};

ActivitiesTitleState resolveTitleState(const bool hasSync, const bool offline, const std::optional<SyncStatus> runtimeStatus,
                                       const bool hasActivities) {
    if (!hasSync || !runtimeStatus.has_value()) {
        return ActivitiesTitleState::Loading;
    }

    switch (*runtimeStatus) {
        case SyncStatus::Undefined:
        case SyncStatus::EnumEnd:
            return ActivitiesTitleState::Loading;
        case SyncStatus::Idle:
        case SyncStatus::Paused:
            if (offline) {
                return ActivitiesTitleState::Offline;
            }
            break;
        case SyncStatus::Starting:
        case SyncStatus::Running:
        case SyncStatus::PauseAsked:
        case SyncStatus::StopAsked:
            return ActivitiesTitleState::InProgress;
        case SyncStatus::Stopped:
        case SyncStatus::Error:
            break;
    }

    if (!hasActivities) {
        return ActivitiesTitleState::NoActivity;
    }
    return *runtimeStatus == SyncStatus::Idle ? ActivitiesTitleState::Idle : ActivitiesTitleState::Paused;
}

QString titleForState(const ActivitiesTitleState state) {
    switch (state) {
        case ActivitiesTitleState::Loading:
            return {};
        case ActivitiesTitleState::Offline:
            return qtTrId("activitiesTitleOffline");
        case ActivitiesTitleState::InProgress:
            return qtTrId("activitiesTitleInProgress");
        case ActivitiesTitleState::NoActivity:
            return qtTrId("activitiesTitleNoActivity");
        case ActivitiesTitleState::Idle:
            return qtTrId("activitiesTitleIdle");
        case ActivitiesTitleState::Paused:
            return qtTrId("activitiesTitlePause");
    }
    return {};
}

std::optional<SyncPath> safeRelativePath(const SyncPath &path) {
    if (path.has_root_name()) {
        return std::nullopt;
    }
    SyncPath normalizedPath = path.relative_path().lexically_normal();
    if (normalizedPath.empty() || normalizedPath == SyncPath{"."}) {
        return std::nullopt;
    }
    if (std::ranges::any_of(normalizedPath, [](const SyncPath &component) { return component == SyncPath{".."}; })) {
        return std::nullopt;
    }
    return normalizedPath;
}

bool isContainedIn(const SyncPath &root, const SyncPath &candidate) {
    const SyncPath normalizedRoot = root.lexically_normal();
    const SyncPath normalizedCandidate = candidate.lexically_normal();
    auto candidateIt = normalizedCandidate.begin();
    for (auto rootIt = normalizedRoot.begin(); rootIt != normalizedRoot.end(); ++rootIt, ++candidateIt) {
        if (candidateIt == normalizedCandidate.end() || *candidateIt != *rootIt) {
            return false;
        }
    }
    return true;
}

} // namespace

ActivitiesController::ActivitiesController(const ActivityStore &activityStore, const AppCache &appCache,
                                           MainSelectionStore &selectionStore, const NetworkStatusObserver &networkStatusObserver,
                                           ActivityService &activityService, QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _selectionStore(selectionStore),
    _networkStatusObserver(networkStatusObserver),
    _activityService(activityService),
    _model(activityStore, appCache, selectionStore, this) {
    (void) connect(&_model, &ActivityListModel::filterChanged, this, &ActivitiesController::filterChanged);
    (void) connect(&_model, &ActivityListModel::projectionChanged, this, &ActivitiesController::refreshPageState);
    (void) connect(&_selectionStore, &MainSelectionStore::currentContextChanged, this, &ActivitiesController::refreshPageState);
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncRuntimeInfoChanged, this,
                   &ActivitiesController::refreshPageState);
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncStatusChanged, this,
                   &ActivitiesController::refreshPageState);
    (void) connect(&_networkStatusObserver, &NetworkStatusObserver::offlineChanged, this,
                   &ActivitiesController::refreshPageState);
    (void) connect(&_activityService, &ActivityService::shareLinkCopied, this, [this](const GenericId activityLocalId) {
        emit shareLinkCopied(ActivityListModel::activityRowId(activityLocalId));
    });
    (void) connect(&_activityService, &ActivityService::actionFailed, this, [this](const GenericId activityLocalId) {
        emit actionFailed(ActivityListModel::activityRowId(activityLocalId));
    });
    refreshPageState();
}

void ActivitiesController::setFilter(const ActivityListModel::Filter filter) {
    _model.setFilter(filter);
}

void ActivitiesController::openLocal(const QString &rowId) {
    const auto target = _model.actionTarget(rowId);
    if (!target.has_value() || !target->canOpenLocal) {
        qCWarning(lcActivitiesController) << "Local activity action rejected for unavailable row | rowId:" << rowId;
        emit actionFailed(rowId);
        return;
    }
    const auto context = actionSyncContext(*target, rowId);
    const auto relativePath = safeRelativePath(target->relativePath);
    if (!context.has_value() || !relativePath.has_value()) {
        qCWarning(lcActivitiesController) << "Local activity action rejected for invalid path | rowId:" << rowId;
        emit actionFailed(rowId);
        return;
    }

    const SyncPath rootPath = context->syncInfo.localPath().lexically_normal();
    const QFileInfo rootInfo{Path2QStr(rootPath)};
    const SyncPath targetPath = (rootPath / *relativePath).lexically_normal();
    const QFileInfo targetInfo{Path2QStr(targetPath)};
    if (!rootInfo.exists() || !rootInfo.isDir() || !targetInfo.exists()) {
        qCWarning(lcActivitiesController) << "Local activity target is missing or outside the synchronization root"
                                          << "| rowId:" << rowId << "| target:" << Path2QStr(targetPath);
        emit actionFailed(rowId);
        return;
    }

    const auto canonicalRootPath = QStr2Path(rootInfo.canonicalFilePath());
    const auto canonicalTargetPath = QStr2Path(targetInfo.canonicalFilePath());
    if (canonicalRootPath.empty() || canonicalTargetPath.empty() || !isContainedIn(canonicalRootPath, canonicalTargetPath)) {
        qCWarning(lcActivitiesController) << "Local activity target resolves outside the synchronization root"
                                          << "| rowId:" << rowId << "| target:" << Path2QStr(targetPath);
        emit actionFailed(rowId);
        return;
    }

    if (const SyncPath folderToOpen = targetInfo.isDir() ? canonicalTargetPath : canonicalTargetPath.parent_path();
        folderToOpen.empty() || !QDesktopServices::openUrl(QUrl::fromLocalFile(Path2QStr(folderToOpen)))) {
        qCWarning(lcActivitiesController) << "Desktop service failed to open local activity folder"
                                          << "| rowId:" << rowId << "| folder:" << Path2QStr(folderToOpen);
        emit actionFailed(rowId);
    }
}

void ActivitiesController::openOnline(const QString &rowId) {
    const auto target = _model.actionTarget(rowId);
    if (!target.has_value() || !target->canOpenOnline) {
        qCWarning(lcActivitiesController) << "Online activity action rejected for unavailable row | rowId:" << rowId;
        emit actionFailed(rowId);
        return;
    }
    const auto context = actionSyncContext(*target, rowId);
    if (!context.has_value()) {
        emit actionFailed(rowId);
        return;
    }
    _activityService.openOnline(target->activityLocalId, context->drive.dbId(), target->remoteNodeId);
}

void ActivitiesController::copyShareLink(const QString &rowId) {
    const auto target = _model.actionTarget(rowId);
    if (!target.has_value() || !target->canCopyShareLink) {
        qCWarning(lcActivitiesController) << "Share-link activity action rejected for unavailable row | rowId:" << rowId;
        emit actionFailed(rowId);
        return;
    }
    const auto context = actionSyncContext(*target, rowId);
    if (!context.has_value()) {
        emit actionFailed(rowId);
        return;
    }
    _activityService.copyShareLink(target->activityLocalId, context->drive.dbId(), target->remoteNodeId);
}

void ActivitiesController::requestFixErrors(const QString &rowId) const {
    const auto target = _model.actionTarget(rowId);
    if (!target.has_value() || !target->canFixErrors) {
        qCInfo(lcActivitiesController) << "Activity error resolution ignored because the row has no active error"
                                       << "| rowId:" << rowId;
        return;
    }
    qCInfo(lcActivitiesController) << "Activity error resolution is not implemented yet"
                                   << "| rowId:" << rowId << "| activeErrorCount:" << target->activeErrorDbIds.size();
}

void ActivitiesController::requestFixAllErrors() const {
    const auto context = _selectionStore.currentSyncContext();
    qCInfo(lcActivitiesController) << "Global activity error resolution is not implemented yet"
                                   << "| syncDbId:" << _selectionStore.currentSyncDbId()
                                   << "| activeErrorCount:" << (context.has_value() ? context->errors.size() : 0);
}

void ActivitiesController::refreshPageState() {
    const bool nextHasActivities = _model.rowCount() > 0;
    const auto context = _selectionStore.currentSyncContext();
    const qint32 nextErrorCount = context.has_value() ? static_cast<qint32>(context->errors.size()) : 0;
    const auto runtimeInfo = _selectionStore.currentSyncRuntimeInfo();
    const auto runtimeStatus = runtimeInfo.has_value() ? std::make_optional(runtimeInfo->status) : std::nullopt;
    const auto titleState =
            resolveTitleState(context.has_value(), _networkStatusObserver.offline(), runtimeStatus, nextHasActivities);
    const bool nextLoading = titleState == ActivitiesTitleState::Loading;
    const QString nextTitle = titleForState(titleState);

    if (_hasActivities != nextHasActivities) {
        _hasActivities = nextHasActivities;
        emit hasActivitiesChanged();
    }
    if (_errorCount != nextErrorCount) {
        _errorCount = nextErrorCount;
        emit errorCountChanged();
    }
    if (_loading != nextLoading) {
        _loading = nextLoading;
        emit loadingChanged();
    }
    if (_title != nextTitle) {
        _title = nextTitle;
        emit titleChanged();
    }
}

std::optional<SyncContext> ActivitiesController::actionSyncContext(const ActivityListModel::ActionTarget &target,
                                                                   const QString &rowId) const {
    auto context = _appCache.syncContext(target.syncDbId);
    if (!context.has_value() || context->syncInfo.dbId() != target.syncDbId || context->drive.dbId() <= 0) {
        qCWarning(lcActivitiesController) << "Activity action rejected because its synchronization context disappeared"
                                          << "| rowId:" << rowId << "| syncDbId:" << target.syncDbId;
        return std::nullopt;
    }
    return context;
}

} // namespace KDC
