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
import QtQuick.Controls
import kDrive.UI

Column {
    id: root

    required property var controller

    // A folder name is free text and goes through `Text.StyledText` below, where `&`, `<` and `>` would be read as
    // markup.
    function escapedMarkup(value: string): string {
        return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
    }

    width: parent ? parent.width : implicitWidth
    spacing: IKSyncConfiguration.sectionSpacing

    Text {
        width: Math.min(parent.width, IKSyncConfiguration.summaryDescriptionWidth)
        text: qsTrId("onboardingAdvancedSettingsDriveSelectionDescription")
        color: IKColors.textSecondary
        font.pixelSize: IKFonts.bodySize
        wrapMode: Text.WordWrap
    }

    ListView {
        id: drivesList

        width: parent.width
        height: Math.min(contentHeight, IKSyncConfiguration.summaryListMaximumHeight)
        model: root.controller.selectedDrivesModel
        spacing: IKSyncConfiguration.summaryListSpacing
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            policy: drivesList.contentHeight > drivesList.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        }

        delegate: Rectangle {
            id: driveRow

            required property int index
            required property string driveName
            required property color driveColor
            required property string localPath
            required property bool customFolder
            required property bool customSelection

            width: drivesList.width
            height: rowContent.implicitHeight + 2 * IKSyncConfiguration.summaryRowPadding
            radius: IKSyncConfiguration.summaryRowRadius
            color: IKColors.syncConfigurationCardSurface

            Row {
                id: rowContent

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: IKSyncConfiguration.summaryRowPadding
                anchors.rightMargin: IKSyncConfiguration.summaryRowPadding
                spacing: IKSyncConfiguration.summaryRowSpacing

                DriveIconView {
                    width: IKSyncConfiguration.driveIconSize
                    height: width
                    iconColor: driveRow.driveColor
                }

                Column {
                    width: Math.max(0, parent.width - configureButton.width - IKSyncConfiguration.driveIconSize
                                    - 2 * parent.spacing)
                    spacing: IKSyncConfiguration.summaryRowTextSpacing

                    Text {
                        id: driveNameText

                        width: parent.width
                        text: driveRow.driveName
                        color: IKColors.textPrimary
                        font.pixelSize: IKFonts.bodySize
                        font.weight: IKFonts.emphasized
                        elide: Text.ElideRight

                        HoverHandler {
                            id: driveNameHover
                        }

                        IKToolTip {
                            visible: driveNameHover.hovered && driveNameText.truncated
                            text: driveRow.driveName
                            maximumTextWidth: IKSyncConfiguration.tooltipMaximumWidth
                        }
                    }

                    Text {
                        id: locationText

                        width: parent.width
                        text: qsTrId("onboardingAdvancedSettingsDriveSelectionLocationMac")
                              .arg("<b>" + (driveRow.customFolder ? root.escapedMarkup(driveRow.localPath)
                                                                  : qsTrId("labelByDefault")) + "</b>")
                        textFormat: Text.StyledText
                        color: IKColors.textSecondary
                        font.pixelSize: IKFonts.subheadlineSize
                        elide: Text.ElideRight

                        HoverHandler {
                            id: locationHover
                        }

                        IKToolTip {
                            visible: locationHover.hovered && (locationText.truncated || driveRow.customFolder)
                            text: driveRow.localPath
                            maximumTextWidth: IKSyncConfiguration.tooltipMaximumWidth
                        }
                    }

                    Text {
                        width: parent.width
                        text: qsTrId("onboardingAdvancedSettingsDriveSelectionExclusionMac")
                              .arg("<b>" + (driveRow.customSelection ? qsTrId("onboardingExclusionSummarySome")
                                                                     : qsTrId("labelAllkDrive")) + "</b>")
                        textFormat: Text.StyledText
                        color: IKColors.textSecondary
                        font.pixelSize: IKFonts.subheadlineSize
                        elide: Text.ElideRight
                    }
                }

                IKLinkButton {
                    id: configureButton

                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTrId("buttonConfigure")
                    // Every row repeats the same label, so the drive is what tells them apart.
                    Accessible.description: driveRow.driveName
                    actionEnabled: !root.controller.busy
                    onClicked: root.controller.configureDrive(driveRow.index)
                }
            }
        }
    }
}
