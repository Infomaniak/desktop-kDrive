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

// One advanced synchronization of the "Advanced sync" page: a collapsible card with its local and remote locations, its
// synchronized folders, and its deletion.
Rectangle {
    id: root

    required property var syncDbId
    required property string localFolderName
    required property string localPath
    required property string remoteFolderName
    required property string remotePath
    required property bool customSelection
    required property bool blackListLoading
    required property bool blackListLoadFailed
    required property bool deletePending
    required property color driveColor
    property bool expanded: true
    readonly property color headerSurfaceColor: headerButton.down || headerButton.hovered ? IKColors.surfaceTertiary
                                                                                          : IKColors.settingsCardSurface

    signal openLocalFolderRequested
    signal openRemoteFolderRequested
    signal retryBlackListRequested
    signal manageRequested(Item trigger)
    signal deleteRequested(Item trigger)

    // The last row already pads its content below, like the rows of a SettingsGroup: the card only adds its bottom padding
    // when collapsed, to keep the header centered.
    implicitHeight: contentColumn.implicitHeight + IKSettings.groupPadding + (root.expanded ? 0 : IKSettings.groupPadding)
    radius: IKRadius.r12
    color: IKColors.settingsCardSurface

    Column {
        id: contentColumn

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: IKSettings.groupPadding

        RowLayout {
            width: parent.width
            height: IKSettings.rowHeight
            // Leaves a gap between the overflowing hover surface and the delete button.
            spacing: IKSpacing.s12

            Button {
                id: headerButton

                Layout.fillWidth: true
                Layout.fillHeight: true
                padding: 0
                focusPolicy: Qt.StrongFocus
                hoverEnabled: true
                Accessible.name: root.localFolderName
                Accessible.checkable: true
                Accessible.checked: root.expanded
                Accessible.onToggleAction: headerButton.clicked()
                onClicked: root.expanded = !root.expanded

                // The hover surface overflows into the card padding, so the chevron stays aligned with the row titles
                // below it while keeping some room around the glyph.
                background: Item {
                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: -IKSettings.advancedSyncHeaderHoverOutset
                        anchors.rightMargin: -IKSettings.advancedSyncHeaderHoverOutset
                        radius: IKRadius.r8
                        color: root.headerSurfaceColor
                        border.width: headerButton.visualFocus ? 2 : 0
                        border.color: IKColors.accentPrimary
                    }
                }

                contentItem: RowLayout {
                    spacing: IKSpacing.s8

                    IKTintedIcon {
                        Layout.preferredWidth: IKSettings.navigationIconSize
                        Layout.preferredHeight: IKSettings.navigationIconSize
                        source: "qrc:/assets/main/chevron-down.svg"
                        color: IKColors.textSecondary
                        rotation: root.expanded ? 0 : -90
                    }

                    IKTintedIcon {
                        Layout.preferredWidth: IKSettings.userDriveIconSize
                        Layout.preferredHeight: IKSettings.userDriveIconSize
                        source: "qrc:/assets/main/folder.svg"
                        color: root.driveColor
                    }

                    Text {
                        id: folderNameText

                        Layout.fillWidth: true
                        Layout.rightMargin: IKSpacing.s8
                        text: root.localFolderName
                        textFormat: Text.PlainText
                        color: IKColors.textPrimary
                        font.pixelSize: IKFonts.bodySize
                        font.weight: IKFonts.emphasized
                        elide: Text.ElideRight
                    }
                }

                IKToolTip {
                    showRequested: folderNameText.truncated && (headerButton.hovered || headerButton.visualFocus)
                    text: root.localFolderName
                }
            }

            SettingsInfoButton {
                id: deleteButton

                iconSource: "qrc:/assets/main/home/trash.svg"
                text: qsTrId("buttonRemoveSync")
                Accessible.name: text + " " + root.localFolderName
                enabled: !root.deletePending
                onClicked: root.deleteRequested(deleteButton)
            }
        }

        Column {
            width: parent.width
            visible: root.expanded

            Rectangle {
                width: parent.width
                height: 1
                color: IKColors.settingsDivider
            }

            SettingsRow {
                title: qsTrId("labelComputerLocation")

                IKLinkButton {
                    Layout.maximumWidth: root.width * IKSettings.driveManagementPathMaxWidthRatio
                    text: root.localPath
                    Accessible.name: qsTrId("labelComputerLocation") + " " + text
                    onClicked: root.openLocalFolderRequested()
                }
            }

            SettingsRow {
                title: qsTrId("labelRemoteLocation")

                IKLinkButton {
                    id: remoteLocationButton

                    Layout.maximumWidth: root.width * IKSettings.driveManagementPathMaxWidthRatio
                    text: root.remoteFolderName
                    external: true
                    Accessible.name: qsTrId("labelRemoteLocation") + " " + root.remotePath
                    onClicked: root.openRemoteFolderRequested()

                    IKToolTip {
                        targetButton: remoteLocationButton
                        text: root.remotePath
                    }
                }
            }

            SyncSelectionRow {
                title: qsTrId("labelSyncedFolders")
                separator: false
                customSelection: root.customSelection
                blackListLoading: root.blackListLoading
                blackListLoadFailed: root.blackListLoadFailed
                accessibleContext: root.localFolderName
                onRetryRequested: root.retryBlackListRequested()
                onManageRequested: trigger => root.manageRequested(trigger)
            }
        }
    }
}
