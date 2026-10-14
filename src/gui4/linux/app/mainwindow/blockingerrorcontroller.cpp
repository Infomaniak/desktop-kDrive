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


#include "app/mainwindow/blockingerrorcontroller.h"

#include "app/appconstants.h"
#include "app/cache/mainselectionstore.h"
#include "app/cache/syncerrorsprojection.h"
#include "app/navigation/approuter.h"
#include "app/services/syncservice.h"

#include <QDesktopServices>
#include <QLoggingCategory>
#include <QUrl>

#include <algorithm>
#include <array>
#include <string_view>

namespace KDC {

namespace {

Q_LOGGING_CATEGORY(lcBlockingErrorController, "gui.v4.blockingerror", QtInfoMsg)

void openUrl(const QUrl &url) {
    if (!QDesktopServices::openUrl(url)) {
        qCWarning(lcBlockingErrorController) << "Cannot open URL | url:" << url;
    }
}

// TODO: Renewing a kSuite (`order2/ksuite/<k_suite.id>`) needs an id the server does not fetch yet (drive API,
// `with=k_suite`). Until then, a kSuite drive, or an unknown pack, open shop.infomaniak.com
QUrl renewalUri(const Drive &drive) {
    const auto &packName = drive.packInfo().name();
    constexpr std::array<std::string_view, 4> standaloneDrivePacks{"free", "solo", "team", "pro"};

    if (std::ranges::find(standaloneDrivePacks, packName) != standaloneDrivePacks.end()) {
        return AppConstants::Drive::renewSubscriptionUri(drive.driveId());
    }

    if (packName.starts_with("my_ksuite")) {
        return AppConstants::Drive::myKSuiteRenewalUri();
    }

    return AppConstants::Drive::shopUri();
}

} // namespace

BlockingErrorController::BlockingErrorController(const SyncErrorsProjection &projection, const MainSelectionStore &selectionStore,
                                                 SyncService &syncService, AppRouter &appRouter, QObject *const parent) :
    QObject(parent),
    _projection(projection),
    _selectionStore(selectionStore),
    _syncService(syncService),
    _appRouter(appRouter) {
    (void) connect(&_projection, &SyncErrorsProjection::currentBlockingErrorChanged, this, &BlockingErrorController::refresh);
    (void) connect(&_selectionStore, &MainSelectionStore::currentContextChanged, this, &BlockingErrorController::changed);
    // The view stays loaded when the new synchronization has the same blocking error: its button must follow that one.
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this,
                   &BlockingErrorController::actionPendingChanged);
    (void) connect(&_appRouter, &AppRouter::currentMainTabChanged, this, &BlockingErrorController::keepBlockingErrorTab);
    (void) connect(&_syncService, &SyncService::syncActionPendingChanged, this, [this](const qint64 syncDbId) {
        if (syncDbId == _selectionStore.currentSyncDbId()) {
            emit actionPendingChanged();
        }
    });
    refresh();
}

QString BlockingErrorController::driveName() const {
    const auto context = _selectionStore.currentSyncContext();
    return context.has_value() ? QString::fromStdString(context->drive.name()) : QString();
}

QColor BlockingErrorController::driveColor() const {
    const auto context = _selectionStore.currentSyncContext();
    const QColor color = context.has_value() ? QColor{QString::fromStdString(context->drive.color())} : QColor();
    return color.isValid() ? color : AppConstants::Drive::defaultColor();
}

bool BlockingErrorController::admin() const {
    const auto context = _selectionStore.currentSyncContext();
    return context.has_value() && context->drive.admin();
}

bool BlockingErrorController::actionPending() const {
    return _syncService.isStartSyncPending(_selectionStore.currentSyncDbId());
}

void BlockingErrorController::triggerAction() {
    const auto context = _selectionStore.currentSyncContext();
    if (!context.has_value()) {
        qCWarning(lcBlockingErrorController) << "Blocking error action ignored: no synchronization selected";
        return;
    }

    if (!_blockingError.has_value()) {
        return;
    }

    const DriveId driveId = context->drive.driveId();
    switch (*_blockingError) {
        case BlockingErrorKind::Asleep:
            // Opening the drive on the web wakes it up.
            qCInfo(lcBlockingErrorController) << "Opening the asleep drive on the web | driveId:" << driveId;
            openUrl(AppConstants::WebDrive::destinationUri(driveId, AppConstants::WebDrive::Destination::OnlineDrive));
            return;
        case BlockingErrorKind::NotRenew:
            if (admin()) {
                qCInfo(lcBlockingErrorController) << "Opening the drive renewal page | driveId:" << driveId
                                                  << "| pack:" << QString::fromStdString(context->drive.packInfo().name());
                openUrl(renewalUri(context->drive));
                return;
            }
            restartCurrentSync();
            return;
        case BlockingErrorKind::Maintenance:
        case BlockingErrorKind::AccessDenied:
            restartCurrentSync();
            return;
        case BlockingErrorKind::LoggedOut:
            qCInfo(lcBlockingErrorController) << "Reconnection requested from the blocking error screen";
            emit loginRequested();
            return;
        case BlockingErrorKind::WakingUp:
            // The waking-up screen has no action: the server clears the error once the drive is awake.
            return;
        case BlockingErrorKind::EnumEnd:
            return;
    }
}

void BlockingErrorController::refresh() {
    const auto blockingError = _projection.currentBlockingError();
    if (blockingError == _blockingError) {
        return;
    }

    _blockingError = blockingError;
    if (active()) {
        _appRouter.showBlockingError();
    } else if (_appRouter.currentMainTab() == AppRouter::MainTab::BlockingError) {
        _appRouter.showHome();
    }
    emit changed();
}

void BlockingErrorController::keepBlockingErrorTab() {
    // The sidebar entries are disabled meanwhile; this covers any other navigation request.
    if (active() && _appRouter.currentMainTab() != AppRouter::MainTab::BlockingError) {
        _appRouter.showBlockingError();
    }
}

void BlockingErrorController::restartCurrentSync() {
    const qint64 syncDbId = _selectionStore.currentSyncDbId();
    qCInfo(lcBlockingErrorController) << "Restarting the blocked synchronization | syncDbId:" << syncDbId;
    _syncService.startSync(syncDbId);
}

} // namespace KDC
