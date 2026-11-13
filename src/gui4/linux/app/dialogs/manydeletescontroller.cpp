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
#include "app/fileiconresolver.h"
#include "app/services/commservice.h"
#include "app/services/sentryservice.h"
#include "app/syncconfiguration/localpaths.h"
#include "libcommon/utility/utility.h"

#include <QCollator>
#include <QDesktopServices>
#include <QFileInfo>
#include <QLocale>
#include <QLoggingCategory>
#include <QPointer>
#include <QVariantMap>

#include <algorithm>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcManyDeletesController, "gui.v4.manydeletescontroller", QtInfoMsg)
} // namespace

ManyDeletesController::ManyDeletesController(CommService &commService, AppCache &appCache, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache) {
    (void) connect(&_commService, &CommService::manyDeletesNotification, this, &ManyDeletesController::enqueue);
    // A synchronization context needs its sync, drive, account and user in the cache.
    (void) connect(&_appCache, &AppCache::usersChanged, this, &ManyDeletesController::onCacheContextChanged);
    (void) connect(&_appCache, &AppCache::accountsChanged, this, &ManyDeletesController::onCacheContextChanged);
    (void) connect(&_appCache, &AppCache::drivesChanged, this, &ManyDeletesController::onCacheContextChanged);
    (void) connect(&_appCache, &AppCache::syncsChanged, this, &ManyDeletesController::onCacheContextChanged);
}

ManyDeletesController::Severity ManyDeletesController::severity() const {
    const auto *const notification = currentNotification();
    return notification != nullptr ? notification->severity : Severity::None;
}

quint64 ManyDeletesController::itemCount() const {
    const auto *const notification = currentNotification();
    return notification != nullptr ? static_cast<quint64>(notification->itemCount) : 0;
}

QVariantList ManyDeletesController::deletedItems() const {
    const auto *const notification = currentNotification();
    return notification != nullptr ? notification->deletedItems : QVariantList{};
}

quint64 ManyDeletesController::hiddenItemCount() const {
    const auto *const notification = currentNotification();
    if (notification == nullptr) {
        return 0;
    }

    const auto shownItemCount = static_cast<Count>(notification->deletedItems.size());
    if (notification->itemCount <= shownItemCount) {
        return 0;
    }

    return static_cast<quint64>(notification->itemCount - shownItemCount);
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
                                    const Count itemCount, const QStringList &deletedPaths) {
    const Severity incomingSeverity = severityFromNotificationType(notificationType);
    if (syncDbId <= 0 || incomingSeverity == Severity::None) {
        qCWarning(lcManyDeletesController) << "Invalid mass deletion notification ignored"
                                           << "| syncDbId:" << syncDbId << "| notificationType:" << notificationType;
        SentryService::reportError(QStringLiteral("Invalid mass deletion notification"),
                                   QStringLiteral("syncDbId: %1 | notificationType: %2 | itemCount: %3")
                                           .arg(syncDbId)
                                           .arg(QString::fromStdString(toString(notificationType)))
                                           .arg(itemCount));
        return;
    }

    const auto existing = std::ranges::find_if(
            _notifications, [syncDbId](const Notification &notification) { return notification.syncDbId == syncDbId; });
    if (existing != _notifications.end() && incomingSeverity <= existing->severity) {
        qCDebug(lcManyDeletesController) << "Duplicate mass deletion notification ignored"
                                         << "| syncDbId:" << syncDbId;
        return;
    }

    const Notification incoming{.syncDbId = syncDbId,
                                .severity = incomingSeverity,
                                .itemCount = itemCount,
                                .deletedPaths = deletedPaths,
                                .deletedItems = makeDeletedItems(syncDbId, deletedPaths),
                                .deletedItemsResolved = canResolveDeletedItems(syncDbId, deletedPaths)};
    if (existing != _notifications.end()) {
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
                                    << "| itemCount:" << itemCount << "| pathCount:" << deletedPaths.size()
                                    << "| queueSize:" << _notifications.size();
    if (wasEmpty) {
        setSubmissionFailed(false);
        notifyPresentationChanged();
        emit presentationRequested();
    }
}

// Sorts the deleted paths in natural order, as file managers do ("2" before "10", case ignored), then resolves the
// shown path, the complete path and the icon of each one. The shown path starts with the synchronization folder, named
// as in the synchronization selector, because the warning can come from any synchronization. The server does not tell
// files from folders, so the icon follows the extension only.
QVariantList ManyDeletesController::makeDeletedItems(const SyncDbId syncDbId, QStringList deletedPaths) const {
    QCollator systemCollator(QLocale::system());
    systemCollator.setNumericMode(true);
    systemCollator.setCaseSensitivity(Qt::CaseInsensitive);
    (void) std::ranges::sort(deletedPaths, [&systemCollator](const QString &lhs, const QString &rhs) {
        return systemCollator.compare(lhs, rhs) < 0;
    });

    const auto context = _appCache.syncContext(syncDbId);
    if (!context.has_value() && !deletedPaths.isEmpty()) {
        qCWarning(lcManyDeletesController) << "Mass deletion paths kept relative until the synchronization is in the cache"
                                           << "| syncDbId:" << syncDbId;
    }

    SyncPath syncFolder;
    if (context.has_value()) {
        const SyncPath &syncRoot = context->syncInfo.localPath();
        syncFolder = syncRoot.filename().empty() ? syncRoot : syncRoot.filename();
    }

    const FileIconResolver fileIconResolver;
    QVariantList deletedItems;
    deletedItems.reserve(deletedPaths.size());
    for (const QString &relativePath: std::as_const(deletedPaths)) {
        QString shortPath = relativePath;
        QString displayPath = relativePath;
        if (context.has_value()) {
            shortPath = Path2QStr(syncFolder / QStr2Path(relativePath));
            const SyncPath absolutePath = absoluteSyncPath(context->syncInfo.localPath(), QStr2Path(relativePath));
            displayPath = displayLocalPath(Path2QStr(absolutePath));
        }

        const QString fileName = QFileInfo(relativePath).fileName();
        deletedItems.append(QVariantMap{
                {QStringLiteral("shortPath"), shortPath},
                {QStringLiteral("displayPath"), displayPath},
                {QStringLiteral("iconName"), fileIconResolver.iconName(fileName, NodeType::File)},
        });
    }

    return deletedItems;
}

// Rows built without the synchronization context are only relative paths: they are rebuilt once it is cached.
bool ManyDeletesController::canResolveDeletedItems(const SyncDbId syncDbId, const QStringList &deletedPaths) const {
    return deletedPaths.isEmpty() || _appCache.syncContext(syncDbId).has_value();
}

// A warning can arrive before the cache holds its synchronization, for instance right after launch. Rebuilds the rows
// of every warning still relative once its context is cached. Only `deletedItemsChanged` is emitted, so the dialog keeps
// its checkbox and pending action.
void ManyDeletesController::resolvePendingDeletedItems() {
    bool currentChanged = false;
    for (auto notification = _notifications.begin(); notification != _notifications.end(); ++notification) {
        if (notification->deletedItemsResolved || !canResolveDeletedItems(notification->syncDbId, notification->deletedPaths)) {
            continue;
        }

        notification->deletedItems = makeDeletedItems(notification->syncDbId, notification->deletedPaths);
        notification->deletedItemsResolved = true;
        qCInfo(lcManyDeletesController) << "Mass deletion paths resolved once the synchronization is in the cache"
                                        << "| syncDbId:" << notification->syncDbId;
        if (notification == _notifications.begin()) {
            currentChanged = true;
        }
    }

    if (currentChanged) {
        emit deletedItemsChanged();
    }
}

void ManyDeletesController::onCacheContextChanged() {
    if (!visible()) {
        return;
    }

    resolvePendingDeletedItems();
    emit canOpenTrashChanged();
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
    _commService.requestAcknowledgeManyDeletes(syncDbId, userChoice, [guard, syncDbId, userChoice](const ExitInfo &exitInfo) {
        if (guard.isNull()) {
            return;
        }
        guard->setBusy(false);
        if (!exitInfo) {
            qCWarning(lcManyDeletesController)
                    << "Mass deletion acknowledgement failed"
                    << "| syncDbId:" << syncDbId << "| code:" << exitInfo.code() << "| cause:" << exitInfo.cause();
            SentryService::reportError(QStringLiteral("Mass deletion acknowledgement failed"),
                                       QStringLiteral("syncDbId: %1 | userChoice: %2 | %3")
                                               .arg(syncDbId)
                                               .arg(QString::fromStdString(toString(userChoice)))
                                               .arg(QString::fromStdString(toString(exitInfo))));
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
    // The sync engine reads this flag from the AppState table, not from the application parameters.
    std::string value;
    if (!CommonUtility::appStateValueToString(AppStateValue{std::in_place_type<bool>, false}, value)) {
        qCWarning(lcManyDeletesController) << "Failed to serialize the soft mass deletion warnings flag";
        return;
    }

    _commService.requestSetAppState(AppStateKey::NotifyBeforeDelete, QString::fromStdString(value), [](const ExitInfo &exitInfo) {
        if (exitInfo) {
            return;
        }
        qCWarning(lcManyDeletesController) << "Failed to disable future soft mass deletion warnings"
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
    emit deletedItemsChanged();
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
