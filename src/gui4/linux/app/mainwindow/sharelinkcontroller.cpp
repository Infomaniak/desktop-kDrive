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

#include "app/mainwindow/sharelinkcontroller.h"

#include "app/appconstants.h"
#include "app/cache/appcache.h"
#include "app/cache/mainselectionstore.h"

#include <QDesktopServices>
#include <QLoggingCategory>
#include <QPointer>
#include <QUrl>

#include <utility>

namespace KDC {

namespace {

Q_LOGGING_CATEGORY(lcShareLink, "gui.v4.sharelink", QtInfoMsg)

ShareLinkController::LinkState linkState(const ActivityService::LinkCopyResult result) {
    switch (result) {
        case ActivityService::LinkCopyResult::Copied:
            return ShareLinkController::LinkState::Copied;
        case ActivityService::LinkCopyResult::PermissionDenied:
            return ShareLinkController::LinkState::PermissionDenied;
        case ActivityService::LinkCopyResult::LimitReached:
            return ShareLinkController::LinkState::LimitReached;
        case ActivityService::LinkCopyResult::Failed:
            break;
    }
    return ShareLinkController::LinkState::Failed;
}

} // namespace

ShareLinkController::ShareLinkController(const AppCache &appCache, const MainSelectionStore &selectionStore,
                                         ActivityService &activityService, QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _activityService(activityService) {
    // The dialog shows a node of the previous synchronization.
    (void) connect(&selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this, &ShareLinkController::close);
    (void) connect(&_appCache, &AppCache::drivesChanged, this, [this] {
        if (isOpen() && refreshDriveInfo()) {
            emit changed();
        }
    });
}

void ShareLinkController::open(Target target) {
    if (target.driveDbId <= 0 || target.remoteNodeId.empty()) {
        qCWarning(lcShareLink) << "Share dialog ignored, node unavailable | driveDbId:" << target.driveDbId;
        return;
    }
    ++_generation;
    _target = std::move(target);
    _restrictedState = LinkState::Idle;
    _publicState = LinkState::Idle;
    (void) refreshDriveInfo();
    qCInfo(lcShareLink) << "Share dialog opened | driveDbId:" << _target.driveDbId;
    emit changed();
}

void ShareLinkController::close() {
    if (!isOpen()) {
        return;
    }
    ++_generation;
    _target = {};
    _restrictedState = LinkState::Idle;
    _publicState = LinkState::Idle;
    _upsellOffer = Offer::None;
    _publicLinkLimit = 0;
    emit changed();
}

void ShareLinkController::copyRestricted() {
    copy(ActivityService::LinkKind::Restricted);
}

void ShareLinkController::copyPublic() {
    copy(ActivityService::LinkKind::Public);
}

void ShareLinkController::openUpsellPage() const {
    const auto drive = isOpen() ? _appCache.drive(_target.driveDbId) : std::nullopt;
    if (!drive.has_value() || UpsellOffers::shareLinkLimitOfferFor(*drive) == Offer::None) {
        qCWarning(lcShareLink) << "Upgrade page ignored: no offer for the drive | driveDbId:" << _target.driveDbId;
        return;
    }

    const QUrl url = AppConstants::Drive::upsellUri(*drive);
    qCInfo(lcShareLink) << "Opening the upgrade page | driveId:" << drive->driveId() << "| url:" << url;
    if (!QDesktopServices::openUrl(url)) {
        qCWarning(lcShareLink) << "Desktop service failed to open the upgrade page | url:" << url;
    }
}

void ShareLinkController::copy(const ActivityService::LinkKind kind) {
    LinkState &state = stateOf(kind);
    // A refused link stays refused for this opening: its button is disabled.
    if (!isOpen() || state == LinkState::Creating || state == LinkState::PermissionDenied || state == LinkState::LimitReached) {
        return;
    }
    state = LinkState::Creating;
    emit changed();

    const QPointer<ShareLinkController> guard{this};
    const uint64_t generation = _generation;
    _activityService.copyLink(kind, _target.driveDbId, _target.remoteNodeId,
                              [guard, generation, kind](const ActivityService::LinkCopyResult result) {
                                  if (guard.isNull() || guard->_generation != generation) {
                                      return;
                                  }
                                  guard->onCopied(kind, result);
                              });
}

void ShareLinkController::onCopied(const ActivityService::LinkKind kind, const ActivityService::LinkCopyResult result) {
    stateOf(kind) = linkState(result);
    emit changed();
}

ShareLinkController::LinkState &ShareLinkController::stateOf(const ActivityService::LinkKind kind) {
    return kind == ActivityService::LinkKind::Public ? _publicState : _restrictedState;
}

bool ShareLinkController::refreshDriveInfo() {
    const auto drive = _appCache.drive(_target.driveDbId);
    const Offer offer = drive.has_value() ? UpsellOffers::shareLinkLimitOfferFor(*drive) : Offer::None;
    const qint64 limit = drive.has_value() ? static_cast<qint64>(drive->sharedLinkQuota()) : 0;
    if (offer == _upsellOffer && limit == _publicLinkLimit) {
        return false;
    }
    _upsellOffer = offer;
    _publicLinkLimit = limit;
    return true;
}

} // namespace KDC
