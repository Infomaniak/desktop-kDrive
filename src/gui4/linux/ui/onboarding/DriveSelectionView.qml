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

import QtQuick
import QtQuick.Controls
import kDrive.UI

Item {
    id: root

    required property var selectionController
    required property var drivesModel

    readonly property bool compact: width < IKOnboarding.driveSelectionCompactBreakpointWidth

    Column {
        width: Math.min(IKOnboarding.driveSelectionContentMaxWidth, root.width - IKSpacing.s64)
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: root.compact ? IKSpacing.s32 : IKOnboarding.driveSelectionContentExpandedLeftMargin
        spacing: IKOnboarding.driveSelectionSectionSpacing

        Text {
            width: parent.width
            text: qsTr("Welcome back!")
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.largeTitleSize
            font.weight: Font.Bold
            lineHeightMode: Text.FixedHeight
            lineHeight: IKOnboarding.driveSelectionTitleLineHeight
            wrapMode: Text.WordWrap
        }

        Rectangle {
            width: Math.min(IKOnboarding.driveSelectionUserBadgeMaxWidth, parent.width)
            height: IKOnboarding.driveSelectionUserBadgeHeight
            radius: IKOnboarding.driveSelectionUserBadgeRadius
            color: IKColors.onboardingUserBadgeSurface

            LabeledAvatarView {
                anchors.fill: parent
                anchors.leftMargin: IKOnboarding.driveSelectionUserBadgeLeftPadding
                anchors.rightMargin: IKOnboarding.driveSelectionUserBadgeRightPadding
                anchors.topMargin: IKOnboarding.driveSelectionUserBadgeVerticalPadding
                anchors.bottomMargin: IKOnboarding.driveSelectionUserBadgeVerticalPadding
                label: root.selectionController.userName
                avatarSource: root.selectionController.userAvatarSource
                avatarMaskColor: parent.color
            }
        }

        Column {
            width: parent.width
            spacing: IKSpacing.s8
            visible: root.selectionController.loading

            Text {
                width: parent.width
                text: qsTr("Loading your kDrives…")
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                lineHeightMode: Text.FixedHeight
                lineHeight: IKOnboarding.driveSelectionDriveNameLineHeight
                wrapMode: Text.WordWrap
            }
        }

        NoDriveAvailableView {
            selectionController: root.selectionController
            visible: root.selectionController.empty && !root.selectionController.loading
            width: parent.width
        }

        Column {
            width: parent.width
            spacing: IKSpacing.s8
            visible: root.selectionController.loadFailed && !root.selectionController.loading

            Text {
                width: parent.width
                text: qsTr("Unable to load your kDrives.")
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                lineHeightMode: Text.FixedHeight
                lineHeight: IKOnboarding.driveSelectionDriveNameLineHeight
                wrapMode: Text.WordWrap
            }

            Button {
                id: retryButton

                height: IKOnboarding.driveSelectionButtonHeight
                text: qsTr("Retry")
                onClicked: root.selectionController.reload()

                contentItem: Text {
                    text: retryButton.text
                    color: IKColors.actionOnPrimary
                    font.pixelSize: IKFonts.bodySize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    implicitWidth: IKOnboarding.driveSelectionButtonMinWidth
                    implicitHeight: IKOnboarding.driveSelectionButtonHeight
                    radius: IKOnboarding.buttonCornerRadius
                    color: IKColors.actionPrimary
                }

                padding: 0
                leftPadding: IKSpacing.s16
                rightPadding: IKSpacing.s16
                topPadding: 0
                bottomPadding: 0
            }
        }

        Column {
            width: parent.width
            spacing: IKSpacing.s8
            visible: !root.selectionController.empty && !root.selectionController.loadFailed && !root.selectionController.loading

            Text {
                width: IKOnboarding.driveSelectionListWidth
                text: qsTr("Select the kDrive to be synchronized on this computer:")
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.subheadlineSize
                font.weight: IKFonts.emphasized
                lineHeightMode: Text.FixedHeight
                lineHeight: IKOnboarding.driveSelectionListTitleLineHeight
                wrapMode: Text.WordWrap
            }

            DrivesListView {
                drivesModel: root.drivesModel
                width: IKOnboarding.driveSelectionListWidth
            }

            Row {
                spacing: IKSpacing.s8

                Item {
                    implicitWidth: advancedButton.implicitWidth
                    implicitHeight: IKOnboarding.driveSelectionButtonHeight

                    Button {
                        id: advancedButton

                        anchors.fill: parent
                        enabled: false
                        text: qsTr("Advanced settings")
                        onClicked: root.selectionController.requestAdvancedSettings()

                        contentItem: Text {
                            text: advancedButton.text
                            color: IKColors.actionDisabled
                            font.pixelSize: IKFonts.bodySize
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        background: Rectangle {
                            implicitWidth: IKOnboarding.driveSelectionSecondaryButtonMinWidth
                            implicitHeight: IKOnboarding.driveSelectionButtonHeight
                            radius: IKOnboarding.buttonCornerRadius
                            color: "transparent"
                        }

                        padding: 0
                        leftPadding: IKSpacing.s16
                        rightPadding: IKSpacing.s16
                        topPadding: 0
                        bottomPadding: 0
                    }

                    HoverHandler {
                        id: advancedButtonHoverHandler
                    }

                    ToolTip {
                        id: advancedButtonTooltip

                        visible: advancedButtonHoverHandler.hovered
                        delay: IKOnboarding.driveSelectionTooltipDelay
                        timeout: -1
                        text: qsTr("Not available yet.")
                        padding: IKOnboarding.driveSelectionTooltipPadding

                        contentItem: Text {
                            width: Math.min(implicitWidth, IKOnboarding.driveSelectionTooltipMaxWidth)
                            text: advancedButtonTooltip.text
                            color: IKColors.onboardingTooltipText
                            font.pixelSize: IKFonts.bodySize
                            lineHeightMode: Text.FixedHeight
                            lineHeight: IKOnboarding.driveSelectionDriveNameLineHeight
                            wrapMode: Text.WordWrap
                        }

                        background: Rectangle {
                            radius: IKOnboarding.driveSelectionTooltipRadius
                            color: IKColors.onboardingTooltipSurface
                        }
                    }
                }

                Button {
                    id: continueButton

                    enabled: root.selectionController.canContinue
                    height: IKOnboarding.driveSelectionButtonHeight
                    text: qsTr("Continue")
                    onClicked: root.selectionController.continueOnboarding()

                    contentItem: Text {
                        text: continueButton.text
                        color: continueButton.enabled ? IKColors.actionOnPrimary : IKColors.actionDisabled
                        font.pixelSize: IKFonts.bodySize
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    background: Rectangle {
                        implicitWidth: IKOnboarding.driveSelectionButtonMinWidth
                        implicitHeight: IKOnboarding.driveSelectionButtonHeight
                        radius: IKOnboarding.buttonCornerRadius
                        color: continueButton.enabled ? IKColors.actionPrimary : IKColors.actionDisabled
                    }

                    padding: 0
                    leftPadding: IKSpacing.s16
                    rightPadding: IKSpacing.s16
                    topPadding: 0
                    bottomPadding: 0
                }
            }
        }
    }
}
