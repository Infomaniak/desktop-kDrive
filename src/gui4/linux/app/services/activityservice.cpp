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

#include "app/services/activityservice.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QPointer>
#include <QUrl>

namespace {
constexpr char serviceKeyActivity[] = "activity";
constexpr char actionOpenOnline[] = "openOnline";

bool isSupportedWebUrl(const QUrl &url) {
    return url.isValid() && !url.isEmpty() && !url.host().isEmpty() &&
           (url.scheme() == QStringLiteral("https") || url.scheme() == QStringLiteral("http"));
}
} // namespace

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcActivityService, "gui.v4.activityservice", QtInfoMsg)

bool validateActivityTarget(const ServiceActionTracker::ActionKey &actionKey, const GenericId activityLocalId,
                            const DriveDbId driveDbId, const NodeId &remoteNodeId) {
    if (activityLocalId > 0 && driveDbId > 0 && !remoteNodeId.empty()) {
        return true;
    }
    qCWarning(lcActivityService) << "Activity action rejected because its target is incomplete"
                                 << "| action:" << actionKey << "| activityLocalId:" << activityLocalId
                                 << "| driveDbId:" << driveDbId << "| remoteNodeId:" << QString::fromStdString(remoteNodeId);
    return false;
}

const char *linkKindName(const ActivityService::LinkKind kind) {
    return kind == ActivityService::LinkKind::Public ? "public" : "restricted";
}

ActivityService::LinkCopyResult linkFailureResult(const ExitInfo &exitInfo) {
    if (exitInfo.code() != ExitCode::BackError) {
        return ActivityService::LinkCopyResult::Failed;
    }
    if (exitInfo.cause() == ExitCause::HttpErrForbidden) {
        return ActivityService::LinkCopyResult::PermissionDenied;
    }
    if (exitInfo.cause() == ExitCause::ShareLinkLimitReached) {
        return ActivityService::LinkCopyResult::LimitReached;
    }
    return ActivityService::LinkCopyResult::Failed;
}
} // namespace

ActivityService::ActivityService(CommService &commService, ServiceActionTracker &serviceActionTracker,
                                 ServiceEventBus &serviceEventBus, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _serviceActionTracker(serviceActionTracker),
    _serviceEventBus(serviceEventBus) {
    (void) connect(&_serviceActionTracker, &ServiceActionTracker::servicePendingChanged, this,
                   [this](const ServiceActionTracker::ServiceKey &serviceKey, const bool) {
                       if (serviceKey == serviceKeyActivity) {
                           emit loadingChanged();
                       }
                   });
}

bool ActivityService::loading() const {
    return _serviceActionTracker.isServicePending(serviceKeyActivity);
}

void ActivityService::openOnline(const GenericId activityLocalId, const DriveDbId driveDbId, const NodeId &remoteNodeId) {
    if (!validateActivityTarget(actionOpenOnline, activityLocalId, driveDbId, remoteNodeId)) {
        emit actionFailed(activityLocalId);
        return;
    }
    if (!beginAction(actionOpenOnline, activityLocalId)) {
        return;
    }

    const QPointer<ActivityService> guard{this};
    _commService.requestSyncGetPrivateLinkUrl(driveDbId, remoteNodeId,
                                              [guard, activityLocalId](const ExitInfo &exitInfo, const QString &urlText) {
                                                  if (guard.isNull()) {
                                                      return;
                                                  }
                                                  guard->handlePrivateLinkUrl(exitInfo, urlText, activityLocalId);
                                              });
}

void ActivityService::copyLink(const LinkKind kind, const DriveDbId driveDbId, const NodeId &remoteNodeId,
                               const LinkCopyCallback &callback) {
    if (driveDbId <= 0 || remoteNodeId.empty()) {
        qCWarning(lcActivityService) << "Link copy rejected because its target is incomplete | kind:" << linkKindName(kind)
                                     << "| driveDbId:" << driveDbId << "| remoteNodeId:" << QString::fromStdString(remoteNodeId);
        callback(LinkCopyResult::Failed);
        return;
    }

    const QPointer<ActivityService> guard{this};
    const auto onReply = [guard, kind, callback](const ExitInfo &exitInfo, const QString &urlText) {
        if (guard.isNull()) {
            return;
        }
        handleLinkUrl(kind, exitInfo, urlText, callback);
    };
    if (kind == LinkKind::Public) {
        _commService.requestSyncGetPublicLinkUrl(driveDbId, remoteNodeId, onReply);
    } else {
        _commService.requestSyncGetPrivateLinkUrl(driveDbId, remoteNodeId, onReply);
    }
}

void ActivityService::handlePrivateLinkUrl(const ExitInfo &exitInfo, const QString &urlText, const GenericId activityLocalId) {
    endAction(actionOpenOnline, activityLocalId);
    if (!exitInfo) {
        notifyRequestFailure(exitInfo, RequestNum::SYNC_GETPRIVATELINKURL, activityLocalId);
        return;
    }

    const QUrl url{urlText};
    if (!isSupportedWebUrl(url)) {
        qCWarning(lcActivityService) << "Private activity URL is empty or invalid"
                                     << "| activityLocalId:" << activityLocalId;
        emit actionFailed(activityLocalId);
        return;
    }
    if (!QDesktopServices::openUrl(url)) {
        qCWarning(lcActivityService) << "Desktop service failed to open private activity URL"
                                     << "| activityLocalId:" << activityLocalId;
        emit actionFailed(activityLocalId);
    }
}

void ActivityService::handleLinkUrl(const LinkKind kind, const ExitInfo &exitInfo, const QString &urlText,
                                    const LinkCopyCallback &callback) {
    if (!exitInfo) {
        qCWarning(lcActivityService) << "Link request failed | kind:" << linkKindName(kind) << "| code:" << exitInfo.code()
                                     << "| cause:" << exitInfo.cause();
        callback(linkFailureResult(exitInfo));
        return;
    }

    const QUrl url{urlText};
    if (!isSupportedWebUrl(url)) {
        qCWarning(lcActivityService) << "Link URL is empty or invalid | kind:" << linkKindName(kind);
        callback(LinkCopyResult::Failed);
        return;
    }
    auto *const clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        qCWarning(lcActivityService) << "Clipboard unavailable for the link | kind:" << linkKindName(kind);
        callback(LinkCopyResult::Failed);
        return;
    }
    clipboard->setText(url.toString());
    callback(LinkCopyResult::Copied);
}

bool ActivityService::beginAction(const ServiceActionTracker::ActionKey &actionKey,
                                  const ServiceActionTracker::ScopeId scopeId) const {
    if (_serviceActionTracker.isActionPending(serviceKeyActivity, actionKey, scopeId)) {
        qCDebug(lcActivityService) << "Duplicate activity action ignored"
                                   << "| action:" << actionKey << "| scopeId:" << scopeId;
        return false;
    }
    _serviceActionTracker.beginAction(serviceKeyActivity, actionKey, scopeId);
    return true;
}

void ActivityService::endAction(const ServiceActionTracker::ActionKey &actionKey,
                                const ServiceActionTracker::ScopeId scopeId) const {
    _serviceActionTracker.endAction(serviceKeyActivity, actionKey, scopeId);
}

void ActivityService::notifyRequestFailure(const ExitInfo &exitInfo, const RequestNum requestNum,
                                           const GenericId activityLocalId) {
    qCWarning(lcActivityService) << "Activity service request failed"
                                 << "| request:" << static_cast<int32_t>(requestNum) << "| code:" << exitInfo.code()
                                 << "| cause:" << exitInfo.cause() << "| activityLocalId:" << activityLocalId;
    _serviceEventBus.notifyGenericError(exitInfo, requestNum);
    emit actionFailed(activityLocalId);
}

} // namespace KDC
