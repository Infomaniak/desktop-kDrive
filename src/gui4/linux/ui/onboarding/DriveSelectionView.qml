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
                label: root.drivesModel.userName
                avatarSource: root.drivesModel.userAvatarSource
                avatarMaskColor: parent.color
            }
        }

        Column {
            width: parent.width
            spacing: IKSpacing.s8
            visible: root.drivesModel.loading

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
            drivesModel: root.drivesModel
            visible: root.drivesModel.empty && !root.drivesModel.loading
            width: parent.width
        }

        Column {
            width: parent.width
            spacing: IKSpacing.s8
            visible: root.drivesModel.loadFailed && !root.drivesModel.loading

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
                onClicked: root.drivesModel.reload()

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
            visible: !root.drivesModel.empty && !root.drivesModel.loadFailed && !root.drivesModel.loading

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

                Button {
                    id: advancedButton

                    enabled: root.drivesModel.canOpenAdvancedSettings
                    height: IKOnboarding.driveSelectionButtonHeight
                    text: qsTr("Advanced settings")
                    onClicked: root.drivesModel.requestAdvancedSettings()

                    contentItem: Text {
                        text: advancedButton.text
                        color: advancedButton.enabled ? IKColors.actionPrimary : IKColors.actionDisabled
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

                Button {
                    id: continueButton

                    enabled: root.drivesModel.canContinue
                    height: IKOnboarding.driveSelectionButtonHeight
                    text: qsTr("Continue")
                    onClicked: root.drivesModel.continueOnboarding()

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
