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

#pragma once

#include "app/services/commservice.h"
#include "app/services/serviceactiontracker.h"
#include "app/services/serviceeventbus.h"
#include "libcommon/utility/types.h"

#include <QObject>

namespace KDC {

/**
 * High-level facade for asynchronous actions initiated from an activity row.
 *
 * It owns link resolution, browser and clipboard side effects, pending-action tracking, and backend failure reporting.
 * Activity projection and local-path validation remain outside this service.
 */
class ActivityService final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

    public:
        explicit ActivityService(CommService &commService, ServiceActionTracker &serviceActionTracker,
                                 ServiceEventBus &serviceEventBus, QObject *parent = nullptr);

        [[nodiscard]] bool loading() const;
        void openOnline(GenericId activityLocalId, DriveDbId driveDbId, const NodeId &remoteNodeId);
        void copyShareLink(GenericId activityLocalId, DriveDbId driveDbId, const NodeId &remoteNodeId);

    signals:
        void loadingChanged();
        void shareLinkCopied(GenericId activityLocalId);
        void actionFailed(GenericId activityLocalId);

    private:
        [[nodiscard]] bool beginAction(const ServiceActionTracker::ActionKey &actionKey, GenericId activityLocalId) const;
        void endAction(const ServiceActionTracker::ActionKey &actionKey, GenericId activityLocalId) const;
        void notifyRequestFailure(const ExitInfo &exitInfo, RequestNum requestNum, GenericId activityLocalId);

        CommService &_commService;
        ServiceActionTracker &_serviceActionTracker;
        ServiceEventBus &_serviceEventBus;
};

} // namespace KDC
