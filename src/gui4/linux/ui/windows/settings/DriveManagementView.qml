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
import QtQuick.Layouts
import kDrive.UI

ScrollView {
    id: root

    required property var controller
    required property var activationController
    required property var driveDbId
    readonly property string navigationTitle: qsTrId("labelkDriveManagement")
    // Reads the notifying preparing/busy properties before targets(), an invokable without change notification, so the
    // binding re-evaluates when an activation starts.
    readonly property bool activationBusy: root.controller.syncCreationPending
                                           || ((root.activationController.preparing || root.activationController.busy)
                                               && root.activationController.targets(root.controller.userDbId,
                                                                                    root.controller.accountId,
                                                                                    root.controller.driveId))

    signal activateRequested(Item trigger)
    signal deleteRequested(Item trigger)

    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    Component.onCompleted: root.controller.open(root.driveDbId)
    Component.onDestruction: root.controller.close(root.driveDbId)

    Column {
        width: root.availableWidth
        padding: IKSettings.pageMargin
        spacing: IKSettings.driveManagementSectionSpacing

        Column {
            width: parent.width - 2 * IKSettings.pageMargin
            spacing: IKSettings.groupSpacing

            Row {
                width: parent.width
                spacing: IKSpacing.s8

                Rectangle {
                    width: IKSettings.driveManagementIconSize
                    height: width
                    radius: IKRadius.r4
                    color: root.controller.driveColor

                    IKTintedIcon {
                        anchors.fill: parent
                        anchors.margins: IKSettings.driveManagementIconGlyphInset
                        source: "qrc:/assets/onboarding/drive-icon-glyph.svg"
                        color: IKColors.settingsDriveGlyph
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - IKSettings.driveManagementIconSize - parent.spacing
                    text: root.controller.driveName
                    textFormat: Text.PlainText
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: IKFonts.emphasized
                    elide: Text.ElideRight
                }
            }

            SettingsGroup {
                width: parent.width

                SettingsRow {
                    visible: root.controller.hasMainSync
                    title: qsTrId("labelSyncLocation")

                    IKLinkButton {
                        Layout.maximumWidth: root.availableWidth * IKSettings.driveManagementPathMaxWidthRatio
                        text: root.controller.localPath
                        Accessible.name: qsTrId("labelSyncLocation") + " " + text
                        onClicked: root.controller.openLocalFolder()
                    }
                }

                SettingsRow {
                    title: qsTrId("labelSynchronisation")
                    separator: false

                    Text {
                        visible: root.controller.hasMainSync ? root.controller.customSelection : true
                        text: root.controller.hasMainSync ? qsTrId("onboardingExclusionSummarySome")
                                                          : qsTrId("notSyncedDrive")
                        color: IKColors.textSecondary
                        font.pixelSize: IKFonts.bodySize
                    }

                    IKModalButton {
                        id: activateButton

                        visible: !root.controller.hasMainSync
                        role: IKModalButton.Tonal
                        text: qsTrId("buttonEnable")
                        Accessible.name: text + " " + qsTrId("labelSynchronisation") + " " + root.controller.driveName
                        actionEnabled: !root.activationBusy
                        busy: root.activationBusy
                        onClicked: root.activateRequested(activateButton)
                    }

                    IKModalButton {
                        visible: root.controller.hasMainSync && root.controller.selectionLoadFailed
                        role: IKModalButton.Tonal
                        text: qsTrId("buttonRetry")
                        Accessible.name: text + " " + qsTrId("labelSynchronisation")
                        onClicked: root.controller.reloadSelection()
                    }

                    IKModalButton {
                        visible: root.controller.hasMainSync && !root.controller.selectionLoadFailed
                        role: IKModalButton.Tonal
                        text: qsTrId("buttonManage")
                        Accessible.name: text + " " + qsTrId("labelSynchronisation")
                        Accessible.description: root.controller.customSelection ? qsTrId("onboardingExclusionSummarySome") : ""
                        busy: root.controller.selectionLoading
                        // The folder selection sub-page comes with the next change of this feature.
                        actionEnabled: false
                    }
                }
            }
        }

        SettingsGroup {
            width: parent.width - 2 * IKSettings.pageMargin
            visible: root.controller.hasMainSync
            contentInset: 0

            SettingsNavigationRow {
                id: deleteRow

                title: qsTrId("buttonRemoveSync")
                destructive: true
                separator: false
                enabled: !root.controller.deletePending
                onNavigationRequested: root.deleteRequested(deleteRow)
            }
        }

        SettingsGroup {
            width: parent.width - 2 * IKSettings.pageMargin
            contentInset: 0

            // The advanced synchronization sub-page comes with a later feature.
            SettingsNavigationRow {
                title: qsTrId("advancedSyncTitle")
                separator: false
                enabled: false
            }
        }
    }
}
