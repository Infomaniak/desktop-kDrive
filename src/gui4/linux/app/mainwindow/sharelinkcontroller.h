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

#include "app/mainwindow/upselloffer.h"
#include "app/services/activityservice.h"
#include "libcommon/utility/types.h"

#include <QObject>
#include <QString>

#include <cstdint>

namespace KDC {

class AppCache;
class MainSelectionStore;

/**
 * QML-facing state of the "Share" dialog of an activity: copies the restricted or the public link of its node.
 *
 * Each link has its own state. The node is fixed when the dialog opens; the public link limit and the upgrade offer
 * follow the drive. Responses of a previous opening are ignored, and the dialog closes when the selected
 * synchronization changes.
 */
class ShareLinkController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool isOpen READ isOpen NOTIFY changed)
        Q_PROPERTY(QString itemName READ itemName NOTIFY changed)
        Q_PROPERTY(QString itemIconName READ itemIconName NOTIFY changed)
        Q_PROPERTY(bool itemIsDirectory READ itemIsDirectory NOTIFY changed)
        Q_PROPERTY(KDC::ShareLinkController::LinkState restrictedState READ restrictedState NOTIFY changed)
        Q_PROPERTY(KDC::ShareLinkController::LinkState publicState READ publicState NOTIFY changed)
        Q_PROPERTY(KDC::UpsellOffers::Offer upsellOffer READ upsellOffer NOTIFY changed)
        Q_PROPERTY(qint64 publicLinkLimit READ publicLinkLimit NOTIFY changed)

    public:
        enum class LinkState : uint8_t {
            Idle,
            Creating,
            Copied,
            PermissionDenied,
            LimitReached,
            Failed,
        };
        Q_ENUM(LinkState)

        struct Target {
                DriveDbId driveDbId{0};
                NodeId remoteNodeId;
                QString name;
                QString iconName;
                bool isDirectory{false};
        };

        ShareLinkController(const AppCache &appCache, const MainSelectionStore &selectionStore, ActivityService &activityService,
                            QObject *parent = nullptr);

        [[nodiscard]] bool isOpen() const { return _target.driveDbId > 0; }
        [[nodiscard]] QString itemName() const { return _target.name; }
        [[nodiscard]] QString itemIconName() const { return _target.iconName; }
        [[nodiscard]] bool itemIsDirectory() const { return _target.isDirectory; }
        [[nodiscard]] LinkState restrictedState() const { return _restrictedState; }
        [[nodiscard]] LinkState publicState() const { return _publicState; }
        /** Offer shown when the public link limit is reached; `None` gives the banner without upgrade. */
        [[nodiscard]] Offer upsellOffer() const { return _upsellOffer; }
        /** Public links included in the drive offer, 0 while unknown. */
        [[nodiscard]] qint64 publicLinkLimit() const { return _publicLinkLimit; }

        void open(Target target);
        Q_INVOKABLE void close();
        Q_INVOKABLE void copyRestricted();
        Q_INVOKABLE void copyPublic();
        /** Opens the shop page that upgrades the drive, when it still has an offer. */
        Q_INVOKABLE void openUpsellPage() const;

    signals:
        void changed();

    private:
        void copy(ActivityService::LinkKind kind);
        void onCopied(ActivityService::LinkKind kind, ActivityService::LinkCopyResult result);
        [[nodiscard]] LinkState &stateOf(ActivityService::LinkKind kind);
        // Returns whether the offer or the limit changed.
        bool refreshDriveInfo();

        const AppCache &_appCache;
        ActivityService &_activityService;
        Target _target;
        LinkState _restrictedState{LinkState::Idle};
        LinkState _publicState{LinkState::Idle};
        Offer _upsellOffer{Offer::None};
        qint64 _publicLinkLimit{0};
        // Responses of a previous opening are ignored.
        uint64_t _generation{0};
};

} // namespace KDC
