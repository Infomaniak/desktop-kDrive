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

Column {
    id: root

    required property var controller

    // Set when the picker returns a path that still has to be validated: the button is disabled meanwhile, so it
    // cannot take the focus back before the controller goes idle again.
    property bool focusRestorePending: false

    function restoreFocus(): void {
        root.focusRestorePending = !changeFolderButton.enabled
        if (changeFolderButton.enabled) {
            changeFolderButton.forceActiveFocus()
        }
    }

    width: parent ? parent.width : implicitWidth
    spacing: IKSyncConfiguration.sectionSpacing

    // The page claims the focus itself: the modal loads it dynamically, so it cannot reach into the page to place it.
    // It lands on the first thing the user can change rather than on the confirmation button.
    Component.onCompleted: Qt.callLater(function() {
        changeFolderButton.forceActiveFocus()
    })

    Connections {
        target: root.controller

        // Returning from the native picker leaves focus nowhere otherwise, since the dialog is a separate window.
        function onCustomFolderDialogClosed() {
            root.restoreFocus()
        }
    }

    Rectangle {
        // Sized from its content so the badge hugs the drive name, as in the reference design.
        width: driveBadgeContent.width + 2 * IKSyncConfiguration.driveBadgePadding
        implicitHeight: driveBadgeContent.implicitHeight + 2 * IKSyncConfiguration.driveBadgePadding
        radius: IKSyncConfiguration.driveBadgeRadius
        color: IKColors.syncConfigurationCardSurface

        Row {
            id: driveBadgeContent

            anchors.left: parent.left
            anchors.leftMargin: IKSyncConfiguration.driveBadgePadding
            anchors.verticalCenter: parent.verticalCenter
            spacing: IKSyncConfiguration.driveBadgeSpacing

            DriveIconView {
                width: IKSyncConfiguration.driveIconSize
                height: width
                anchors.verticalCenter: parent.verticalCenter
                iconColor: root.controller.currentDriveColor
            }

            Text {
                id: driveNameText

                width: Math.min(implicitWidth,
                                Math.max(0, root.width - IKSyncConfiguration.driveIconSize - parent.spacing
                                         - 2 * IKSyncConfiguration.driveBadgePadding))
                anchors.verticalCenter: parent.verticalCenter
                text: root.controller.currentDriveName
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.emphasized
                elide: Text.ElideRight

                HoverHandler {
                    id: driveNameHover
                }

                IKToolTip {
                    visible: driveNameHover.hovered && driveNameText.truncated
                    text: root.controller.currentDriveName
                    maximumTextWidth: IKSyncConfiguration.tooltipMaximumWidth
                }
            }
        }
    }

    Rectangle {
        width: parent.width
        implicitHeight: locationContent.implicitHeight + 2 * IKSyncConfiguration.cardPadding
        radius: IKSyncConfiguration.cardRadius
        color: IKColors.syncConfigurationCardSurface

        Column {
            id: locationContent

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: IKSyncConfiguration.cardPadding
            spacing: IKSyncConfiguration.cardSpacing

            Column {
                width: parent.width
                spacing: IKSyncConfiguration.cardTitleSpacing

                Text {
                    width: parent.width
                    text: qsTrId("labelSyncLocation")
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: IKFonts.emphasized
                }

                Text {
                    width: parent.width
                    text: qsTrId("onboardingAdvancedSettingsDriveCustomizeLocation")
                    color: IKColors.textSecondary
                    font.pixelSize: IKFonts.subheadlineSize
                    wrapMode: Text.WordWrap
                }
            }

            Row {
                width: parent.width
                spacing: IKSyncConfiguration.fieldSpacing

                Rectangle {
                    id: locationKindField

                    width: locationKindContent.implicitWidth + 2 * IKSyncConfiguration.fieldHorizontalPadding
                    height: locationKindContent.implicitHeight + 2 * IKSyncConfiguration.fieldVerticalPadding
                    anchors.verticalCenter: parent.verticalCenter
                    radius: IKSyncConfiguration.fieldRadius
                    color: IKColors.syncConfigurationFieldSurface
                    border.width: IKSyncConfiguration.fieldBorderWidth
                    border.color: IKColors.syncConfigurationFieldBorder

                    Row {
                        id: locationKindContent

                        anchors.centerIn: parent
                        spacing: IKSpacing.s4

                        IKTintedIcon {
                            width: IKSyncConfiguration.folderIconSize
                            height: width
                            anchors.verticalCenter: parent.verticalCenter
                            source: "qrc:/assets/main/folder.svg"
                            color: IKColors.syncConfigurationFolderIcon
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.controller.currentUsesDefaultFolder ? qsTrId("syncFolderDefaultLocation")
                                                                           : qsTrId("syncFolderCustomLocation")
                            color: IKColors.textPrimary
                            font.pixelSize: IKFonts.bodySize
                            font.weight: IKFonts.emphasized
                        }
                    }
                }

                Text {
                    id: pathText

                    width: Math.max(0, parent.width - locationKindField.width - parent.spacing)
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.controller.currentLocalPath
                    color: IKColors.textSecondary
                    font.pixelSize: IKFonts.bodySize
                    elide: Text.ElideMiddle

                    HoverHandler {
                        id: pathHover
                    }

                    IKToolTip {
                        visible: pathHover.hovered && pathText.truncated
                        text: root.controller.currentLocalPath
                        maximumTextWidth: IKSyncConfiguration.tooltipMaximumWidth
                    }
                }
            }

            Row {
                spacing: IKSyncConfiguration.fieldSpacing

                IKModalButton {
                    id: changeFolderButton

                    role: IKModalButton.Primary
                    text: qsTrId("buttonChangeFolder")
                    actionEnabled: !root.controller.busy
                    onClicked: root.controller.requestCustomFolder()
                    onEnabledChanged: {
                        if (enabled && root.focusRestorePending) {
                            root.restoreFocus()
                        }
                    }
                }

                IKModalButton {
                    visible: !root.controller.currentUsesDefaultFolder
                    role: IKModalButton.Primary
                    text: qsTrId("buttonReturnToDefaultFolder")
                    actionEnabled: !root.controller.busy
                    onClicked: root.controller.returnToDefaultFolder()
                }
            }

            SyncConfigurationErrorBlock {
                width: parent.width
                errorText: root.controller.localFolderErrorText
            }

            Text {
                width: parent.width
                visible: root.controller.localFolderErrorText.length === 0
                text: qsTrId("onboardingAdvancedSettingsDriveCustomizeLocationTip")
                color: IKColors.textTertiary
                font.pixelSize: IKFonts.subheadlineSize
                wrapMode: Text.WordWrap
            }
        }
    }

    Rectangle {
        width: parent.width
        implicitHeight: exclusionContent.implicitHeight + 2 * IKSyncConfiguration.cardPadding
        radius: IKSyncConfiguration.cardRadius
        color: IKColors.syncConfigurationCardSurface

        Column {
            id: exclusionContent

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: IKSyncConfiguration.cardPadding
            spacing: IKSyncConfiguration.cardSpacing

            Column {
                width: parent.width
                spacing: IKSyncConfiguration.cardTitleSpacing

                Text {
                    width: parent.width
                    text: qsTrId("onboardingAdvancedSettingsDriveExclusionTitle")
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: IKFonts.emphasized
                }

                Text {
                    width: parent.width
                    text: qsTrId("onboardingAdvancedSettingsDriveExclusionDescription")
                    color: IKColors.textSecondary
                    font.pixelSize: IKFonts.subheadlineSize
                    wrapMode: Text.WordWrap
                }
            }

            Row {
                width: parent.width
                spacing: IKSyncConfiguration.statusSpacing

                IKModalButton {
                    id: selectFoldersButton

                    anchors.verticalCenter: parent.verticalCenter
                    role: IKModalButton.Primary
                    text: qsTrId("buttonSelectFolders")
                    actionEnabled: !root.controller.busy
                    onClicked: root.controller.selectFolders()
                }

                Row {
                    width: Math.max(0, parent.width - selectFoldersButton.width - parent.spacing)
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: IKSpacing.s4

                    IKTintedIcon {
                        width: IKSyncConfiguration.statusIconSize
                        height: width
                        anchors.verticalCenter: parent.verticalCenter
                        source: "qrc:/assets/main/activities/status-synchronized-outline.svg"
                        color: IKColors.syncConfigurationConfirmationIcon
                    }

                    Text {
                        width: Math.max(0, parent.width - IKSyncConfiguration.statusIconSize - parent.spacing)
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.controller.currentHasCustomSelection ? qsTrId("onboardingExclusionSummarySome")
                                                                        : qsTrId("onboardingExclusionSummaryNone")
                        color: IKColors.textSecondary
                        font.pixelSize: IKFonts.bodySize
                        elide: Text.ElideRight
                    }
                }
            }

            Text {
                width: parent.width
                text: qsTrId("selectFoldersToSyncDescription")
                color: IKColors.textTertiary
                font.pixelSize: IKFonts.subheadlineSize
                wrapMode: Text.WordWrap
            }
        }
    }
}
