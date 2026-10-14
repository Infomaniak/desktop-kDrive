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

pragma ComponentBehavior: Bound

import QtQuick
import kDrive.UI

// Replaces the page content while the selected synchronization is blocked. The sidebar stays usable to select another
// synchronization; BlockingErrorController returns to Home once the error is gone.
Item {
    id: root

    required property var controller

    readonly property int kind: controller.kind
    readonly property bool renewsOnWeb: kind === BlockingErrors.NotRenew && controller.admin
    readonly property bool securityBadge: kind === BlockingErrors.Asleep || kind === BlockingErrors.WakingUp
    readonly property bool neutralBadge: kind === BlockingErrors.LoggedOut

    // Actions that restart the synchronization stay busy until the server answers.
    function restartsSync() {
        switch (root.kind) {
        case BlockingErrors.NotRenew:
            return !root.renewsOnWeb;
        case BlockingErrors.Maintenance:
        case BlockingErrors.AccessDenied:
            return true;
        }
        return false;
    }

    function badgeSurfaceColor() {
        if (root.securityBadge) {
            return IKColors.statusLightSecurity;
        }
        return root.neutralBadge ? IKColors.statusLightNeutral : IKColors.statusLightWarning;
    }

    function badgeColor() {
        if (root.securityBadge) {
            return IKColors.statusMediumSecurity;
        }
        return root.neutralBadge ? IKColors.statusMediumNeutral : IKColors.statusMediumWarning;
    }

    function title() {
        switch (root.kind) {
        case BlockingErrors.Asleep:
            return qsTrId("driveAsleepErrorTitle");
        case BlockingErrors.WakingUp:
            return qsTrId("driveWakingUpErrorTitle");
        case BlockingErrors.NotRenew:
            return qsTrId("driveLockedErrorTitle");
        case BlockingErrors.Maintenance:
            return qsTrId("errDriveMaintenanceTitle");
        case BlockingErrors.AccessDenied:
            return qsTrId("driveAccessDeniedErrorTitle");
        case BlockingErrors.LoggedOut:
            return qsTrId("driveLoggingErrorTitle");
        }
        return "";
    }

    function description() {
        switch (root.kind) {
        case BlockingErrors.WakingUp:
            return qsTrId("driveWakingUpErrorDescription");
        case BlockingErrors.NotRenew:
            return root.renewsOnWeb ? qsTrId("driveLockedAdminErrorDescription") : qsTrId("driveLockedErrorDescription");
        case BlockingErrors.Maintenance:
            return qsTrId("errDriveMaintenanceDescription");
        case BlockingErrors.AccessDenied:
            return qsTrId("driveAccessDeniedErrorDescription");
        case BlockingErrors.LoggedOut:
            return qsTrId("driveLoggingErrorDescription");
        }
        return "";
    }

    function actionText() {
        switch (root.kind) {
        case BlockingErrors.Asleep:
            return qsTrId("buttonWakeUp");
        case BlockingErrors.NotRenew:
            return root.renewsOnWeb ? qsTrId("buttonUpdateSubscription") : qsTrId("buttonRefresh");
        case BlockingErrors.Maintenance:
            return qsTrId("buttonRefresh");
        case BlockingErrors.AccessDenied:
            return qsTrId("buttonRetry");
        case BlockingErrors.LoggedOut:
            return qsTrId("buttonLogin");
        }
        return "";
    }

    function badgeName() {
        switch (root.kind) {
        case BlockingErrors.Asleep:
            return "moon-sleep";
        case BlockingErrors.WakingUp:
            return "sun";
        case BlockingErrors.Maintenance:
            return "wrench";
        case BlockingErrors.LoggedOut:
            return "door-arrow-right";
        }
        return "warning";
    }

    IKBlockingState {
        anchors.fill: parent
        anchors.leftMargin: IKBlockingStateTokens.blockingErrorMargin
        anchors.rightMargin: IKBlockingStateTokens.blockingErrorMargin
        anchors.bottomMargin: IKBlockingStateTokens.blockingErrorMargin
        anchors.topMargin: IKBlockingStateTokens.blockingErrorTopMargin
        visualSpacing: IKBlockingStateTokens.blockingErrorVisualSpacing
        title: root.title()
        description: root.description()

        visual: BlockingErrorDriveLabel {
            driveName: root.controller.driveName
            driveColor: root.controller.driveColor
            badgeSource: "qrc:/assets/main/blockingerror/" + root.badgeName() + ".svg"
            badgeSurfaceColor: root.badgeSurfaceColor()
            badgeColor: root.badgeColor()
        }

        actions: [
            // The waking-up screen has no action: the server clears the error once the drive is awake.
            IKLoadingSpinner {
                visible: root.kind === BlockingErrors.WakingUp
                width: IKBlockingStateTokens.spinnerSize
                height: IKBlockingStateTokens.spinnerSize
                strokeWidth: IKBlockingStateTokens.spinnerStrokeWidth
            },
            IKModalButton {
                visible: root.kind !== BlockingErrors.WakingUp
                text: root.actionText()
                // Waking the drive up and renewing the subscription happen on the web.
                external: root.kind === BlockingErrors.Asleep || root.renewsOnWeb
                busy: root.restartsSync() && root.controller.actionPending
                onClicked: root.controller.triggerAction()
            }
        ]
    }
}
