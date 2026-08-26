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

#include "manydeletescontroller.h"

#include "app/appconstants.h"
#include "app/cache/appcache.h"
#include "app/services/commservice.h"
#include "app/services/parametersservice.h"

#include <QDesktopServices>
#include <QLoggingCategory>
#include <QPointer>

#include <algorithm>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcManyDeletesController, "gui.v4.manydeletescontroller", QtInfoMsg)
} // namespace

ManyDeletesController::ManyDeletesController(CommService &commService, AppCache &appCache, ParametersService &parametersService,
                                             QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache),
    _parametersService(parametersService) {
    (void) connect(&_commService, &CommService::manyDeletesNotification, this, &ManyDeletesController::enqueue);
    (void) connect(&_appCache, &AppCache::syncsChanged, this, [this] {
        if (visible()) {
            emit canOpenTrashChanged();
        }
    });
    (void) connect(&_appCache, &AppCache::drivesChanged, this, [this] {
        if (visible()) {
            emit canOpenTrashChanged();
        }
    });
}

ManyDeletesController::Severity ManyDeletesController::severity() const {
    const auto *const notification = currentNotification();
    return notification != nullptr ? notification->severity : Severity::None;
}

quint64 ManyDeletesController::itemCount() const {
    const auto *const notification = currentNotification();
    return notification != nullptr ? static_cast<quint64>(notification->itemCount) : 0;
}

bool ManyDeletesController::canOpenTrash() const {
    const auto *const notification = currentNotification();
    if (notification == nullptr || notification->severity != Severity::Soft) {
        return false;
    }

    const auto context = _appCache.syncContext(notification->syncDbId);
    return context.has_value() && context->drive.driveId() > 0;
}

void ManyDeletesController::dismissSoft(const bool disableFutureWarnings) {
    if (const auto *const notification = currentNotification();
        notification == nullptr || notification->severity != Severity::Soft || _busy) {
        qCWarning(lcManyDeletesController) << "Soft mass deletion dismissal ignored outside a ready soft warning";
        return;
    }

    if (disableFutureWarnings) {
        this->disableFutureWarnings();
    }
    finishCurrent();
}

void ManyDeletesController::openTrash(const bool disableFutureWarnings) {
    const auto *const notification = currentNotification();
    if (notification == nullptr || notification->severity != Severity::Soft || _busy) {
        qCWarning(lcManyDeletesController) << "Open trash ignored outside a ready soft mass deletion warning";
        return;
    }

    const auto context = _appCache.syncContext(notification->syncDbId);
    if (!context.has_value() || context->drive.driveId() <= 0) {
        qCWarning(lcManyDeletesController) << "Open trash ignored because the warning synchronization has no drive context"
                                           << "| syncDbId:" << notification->syncDbId;
        emit canOpenTrashChanged();
        return;
    }

    if (const QUrl trashUrl =
                AppConstants::WebDrive::destinationUri(context->drive.driveId(), AppConstants::WebDrive::Destination::Trash);
        !QDesktopServices::openUrl(trashUrl)) {
        qCWarning(lcManyDeletesController) << "Desktop service failed to open the web trash"
                                           << "| syncDbId:" << notification->syncDbId << "| url:" << trashUrl;
        return;
    }

    if (disableFutureWarnings) {
        this->disableFutureWarnings();
    }
    finishCurrent();
}

void ManyDeletesController::restoreFiles() {
    acknowledge(TooManyDeletesUserChoice::Revert);
}

void ManyDeletesController::deleteOnline() {
    acknowledge(TooManyDeletesUserChoice::Continue);
}

void ManyDeletesController::enqueue(const SyncDbId syncDbId, const TooManyDeletesNotificationType notificationType,
                                    const Count itemCount) {
    const Severity incomingSeverity = severityFromNotificationType(notificationType);
    if (syncDbId <= 0 || incomingSeverity == Severity::None) {
        qCWarning(lcManyDeletesController) << "Invalid mass deletion notification ignored"
                                           << "| syncDbId:" << syncDbId << "| notificationType:" << notificationType;
        return;
    }

    const Notification incoming{.syncDbId = syncDbId, .severity = incomingSeverity, .itemCount = itemCount};
    if (const auto existing = std::ranges::find_if(
                _notifications, [syncDbId](const Notification &notification) { return notification.syncDbId == syncDbId; });
        existing != _notifications.end()) {
        if (incomingSeverity <= existing->severity) {
            qCDebug(lcManyDeletesController) << "Duplicate mass deletion notification ignored"
                                             << "| syncDbId:" << syncDbId;
            return;
        }

        const bool currentReplaced = existing == _notifications.begin();
        *existing = incoming;
        if (currentReplaced) {
            setSubmissionFailed(false);
            notifyPresentationChanged();
        }
        qCInfo(lcManyDeletesController) << "Soft mass deletion warning replaced by hard warning"
                                        << "| syncDbId:" << syncDbId << "| itemCount:" << itemCount;
        return;
    }

    const bool wasEmpty = _notifications.empty();
    _notifications.push_back(incoming);
    qCInfo(lcManyDeletesController) << "Mass deletion warning queued"
                                    << "| syncDbId:" << syncDbId << "| severity:" << incomingSeverity
                                    << "| itemCount:" << itemCount << "| queueSize:" << _notifications.size();
    if (wasEmpty) {
        setSubmissionFailed(false);
        notifyPresentationChanged();
        emit presentationRequested();
    }
}

void ManyDeletesController::acknowledge(const TooManyDeletesUserChoice userChoice) {
    const auto *const notification = currentNotification();
    if (notification == nullptr || notification->severity != Severity::Hard || _busy) {
        qCWarning(lcManyDeletesController) << "Mass deletion acknowledgement ignored outside a ready hard warning";
        return;
    }

    const SyncDbId syncDbId = notification->syncDbId;
    setSubmissionFailed(false);
    setBusy(true);

    const QPointer guard{this};
    _commService.requestAcknowledgeManyDeletes(syncDbId, userChoice, [guard, syncDbId](const ExitInfo &exitInfo) {
        if (guard.isNull()) {
            return;
        }

        guard->setBusy(false);
        if (!exitInfo) {
            qCWarning(lcManyDeletesController)
                    << "Mass deletion acknowledgement failed"
                    << "| syncDbId:" << syncDbId << "| code:" << exitInfo.code() << "| cause:" << exitInfo.cause();
            guard->setSubmissionFailed(true);
            return;
        }

        qCInfo(lcManyDeletesController) << "Mass deletion acknowledgement confirmed"
                                        << "| syncDbId:" << syncDbId;
        guard->finishCurrent();
    });
}

void ManyDeletesController::finishCurrent() {
    if (_notifications.empty()) {
        return;
    }

    _notifications.pop_front();
    setBusy(false);
    setSubmissionFailed(false);
    notifyPresentationChanged();
}

void ManyDeletesController::disableFutureWarnings() const {
    _parametersService.updateParameters([](ParametersInfo &parametersInfo) { parametersInfo.setNotifyBeforeDelete(false); },
                                        [](const ExitInfo &exitInfo) {
                                            if (exitInfo) {
                                                return;
                                            }
                                            qCWarning(lcManyDeletesController)
                                                    << "Failed to disable future soft mass deletion warnings"
                                                    << "| code:" << exitInfo.code() << "| cause:" << exitInfo.cause();
                                        });
}

void ManyDeletesController::setBusy(const bool busy) {
    if (_busy == busy) {
        return;
    }
    _busy = busy;
    emit busyChanged();
}

void ManyDeletesController::setSubmissionFailed(const bool failed) {
    if (_submissionFailed == failed) {
        return;
    }
    _submissionFailed = failed;
    emit submissionFailedChanged();
}

void ManyDeletesController::notifyPresentationChanged() {
    emit presentationChanged();
    emit canOpenTrashChanged();
}

const ManyDeletesController::Notification *ManyDeletesController::currentNotification() const {
    return _notifications.empty() ? nullptr : &_notifications.front();
}

ManyDeletesController::Severity ManyDeletesController::severityFromNotificationType(
        const TooManyDeletesNotificationType notificationType) {
    switch (notificationType) {
        case TooManyDeletesNotificationType::SoftLimit:
            return Severity::Soft;
        case TooManyDeletesNotificationType::HardLimit:
            return Severity::Hard;
        case TooManyDeletesNotificationType::Unknown:
        case TooManyDeletesNotificationType::EnumEnd:
            return Severity::None;
    }
    return Severity::None;
}

} // namespace KDC
