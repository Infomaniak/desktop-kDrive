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

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import kDrive.UI

// "Share" dialog of an activity: copies the restricted or the public link of its node. It follows `controller`
// (ShareLinkController): it opens when the controller opens and closes with it.
IKModal {
    id: root

    required property var controller

    readonly property int publicState: root.controller.publicState
    readonly property bool limitReached: root.publicState === ShareLinkController.LimitReached
    readonly property bool hasUpsellOffer: root.controller.upsellOffer !== UpsellOffers.None
    readonly property string limitText: root.controller.publicLinkLimit > 0
                                        ? qsTrId("shareLinkLimitReachedDescription").arg(root.controller.publicLinkLimit)
                                        : qsTrId("shareLinkLimitReachedGenericDescription")

    signal fallbackFocusRequested

    title: qsTrId("buttonShare")
    dialogId: "main.activities.shareDialog"
    iconSource: "qrc:/assets/taskbar/logo_kdrive.svg"
    iconTinted: false
    escapeDismissible: true
    actionsStacked: true
    preferredWidth: IKActivities.shareDialogWidth
    initialFocusItem: closeButton
    onDismissRequested: root.controller.close()
    // The view can go away while the dialog is open (tab change, window hidden): the next "Share" must open it again.
    Component.onDestruction: root.controller.close()
    onClosed: {
        root.controller.close();
        root.fallbackFocusRequested();
    }

    Connections {
        target: root.controller

        function onChanged() {
            // `visible` rather than `opened`, which stays false during the opening transition.
            if (root.controller.isOpen && !root.visible) {
                root.open();
            } else if (!root.controller.isOpen && root.visible) {
                root.close();
            }
        }
    }

    bodyData: ColumnLayout {
        id: content

        width: parent.width
        spacing: IKModalTokens.bodySpacing

        ErrorItemChip {
            Layout.maximumWidth: content.width
            name: root.controller.itemName
            path: ""
            iconName: root.controller.itemIconName
            isDirectory: root.controller.itemIsDirectory
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: IKActivities.shareLinkCardSpacing

            // Instant: the restricted link is built without network request, so it shows no progress; the controller
            // ignores a click while the reply is pending.
            ShareLinkRow {
                id: restrictedRow

                readonly property int linkState: root.controller.restrictedState

                Layout.fillWidth: true
                title: qsTrId("shareLinkRestrictedTitle")
                description: qsTrId("shareLinkRestrictedDescription")
                iconSource: "qrc:/assets/main/activities/share/lock.svg"
                statusText: restrictedRow.linkState === ShareLinkController.Copied ? qsTrId("shareLinkCopied") : ""
                hasNotice: restrictedRow.linkState === ShareLinkController.Failed
                actionId: root.dialogId + ".copyRestricted"
                onCopyRequested: root.controller.copyRestricted()

                ShareLinkFailureNotice {
                    visible: restrictedRow.hasNotice
                }
            }

            ShareLinkRow {
                id: publicRow

                Layout.fillWidth: true
                title: qsTrId("shareLinkPublicTitle")
                description: qsTrId("shareLinkPublicDescription")
                iconSource: "qrc:/assets/main/activities/share/globe.svg"
                statusText: root.publicState === ShareLinkController.Copied ? qsTrId("shareLinkCopied") : ""
                copyBusy: root.publicState === ShareLinkController.Creating
                // A refused link stays refused while the dialog is open; a failure can be retried.
                copyEnabled: root.publicState !== ShareLinkController.PermissionDenied && !root.limitReached
                hasNotice: root.publicState === ShareLinkController.PermissionDenied || root.limitReached
                           || root.publicState === ShareLinkController.Failed
                actionId: root.dialogId + ".copyPublic"
                onCopyRequested: root.controller.copyPublic()

                ShareLinkNotice {
                    visible: root.publicState === ShareLinkController.PermissionDenied
                    title: qsTrId("shareLinkPermissionDeniedTitle")
                    text: qsTrId("shareLinkPermissionDeniedDescription")
                    iconSource: "qrc:/assets/main/activities/share/stop-hand.svg"
                }
                // Without upgrade: the user is not an administrator, or the drive already has the highest offer.
                ShareLinkNotice {
                    visible: root.limitReached && !root.hasUpsellOffer
                    title: qsTrId("shareLinkLimitReachedTitle")
                    text: root.limitText
                    iconSource: "qrc:/assets/main/triangle-alert.svg"
                }
                ShareLinkUpsellBanner {
                    visible: root.limitReached && root.hasUpsellOffer
                    kSuitePro: root.controller.upsellOffer === UpsellOffers.KSuitePro
                    title: qsTrId("shareLinkLimitReachedTitle")
                    text: root.limitText
                    actionId: root.dialogId + ".upgrade"
                    onUpgradeRequested: root.controller.openUpsellPage()
                }
                ShareLinkFailureNotice {
                    visible: root.publicState === ShareLinkController.Failed
                }
            }
        }
    }

    footerData: [
        IKModalButton {
            id: closeButton

            Layout.fillWidth: true
            text: qsTrId("buttonClose")
            role: IKModalButton.Tonal
            actionId: root.dialogId + ".close"
            onClicked: root.controller.close()
        }
    ]

    // Any other failure: Copy stays enabled to retry.
    component ShareLinkFailureNotice: ShareLinkNotice {
        title: qsTrId("failedToCreateShareLinkError")
        iconSource: "qrc:/assets/main/triangle-alert.svg"
    }
}
