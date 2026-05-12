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
import QtQuick.Layouts
import kDrive.UI

// THIS UI IS FOR TEST ONLY, WILL BE DELETED ON THE NEXT PR

Window {
    id: mainWindow

    function errorLabel(hasError, count) {
        if (!hasError)
            return "OK";
        return `${count} error${count === 1 ? "" : "s"}`;
    }
    function sizeLabel(value) {
        const numberValue = Number(value);
        if (!Number.isFinite(numberValue) || numberValue <= 0)
            return "0 B";
        const units = ["B", "KB", "MB", "GB", "TB"];
        let unitIndex = 0;
        let scaled = numberValue;
        while (scaled >= 1024 && unitIndex < units.length - 1) {
            scaled = scaled / 1024;
            unitIndex += 1;
        }
        return `${scaled.toFixed(unitIndex === 0 ? 0 : 1)} ${units[unitIndex]}`;
    }
    function initials(name, email) {
        const displayName = String(name || "").trim();
        if (displayName.length > 0) {
            const parts = displayName.split(/\s+/);
            const first = parts[0].charAt(0);
            const second = parts.length > 1 ? parts[parts.length - 1].charAt(0) : "";
            return `${first}${second}`.toUpperCase();
        }

        const address = String(email || "").trim();
        return address.length > 0 ? address.charAt(0).toUpperCase() : "?";
    }

    color: IKColors.surfacePrimary
    height: 760
    minimumHeight: 620
    minimumWidth: 920
    title: "kDrive"
    visible: true
    width: 1180

    Rectangle {
        anchors.fill: parent
        color: IKColors.surfacePrimary

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: IKSpacing.page
            spacing: IKSpacing.s16

            RowLayout {
                Layout.fillWidth: true
                spacing: IKSpacing.s16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: IKSpacing.s4

                    Text {
                        color: IKColors.textPrimary
                        font.pixelSize: 26
                        font.weight: Font.DemiBold
                        text: "Cache snapshot"
                    }
                    Text {
                        Layout.fillWidth: true
                        color: IKColors.textSecondary
                        elide: Text.ElideRight
                        text: currentSyncModel.hasSync ? `${currentSyncModel.userEmail} / ${currentSyncModel.driveName} / sync #${currentSyncModel.syncDbId}` : "No current sync selected"
                    }
                }
                Rectangle {
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: 140
                    border.color: IKColors.statusMediumWarning
                    border.width: 1
                    color: IKColors.statusLightWarning
                    radius: 8

                    Text {
                        anchors.centerIn: parent
                        color: IKColors.statusStrongWarning
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        text: "TEST ONLY UI"
                    }
                }
                Button {
                    text: ThemeMode._mode === ThemeMode.System ? "System" : ThemeMode._mode === ThemeMode.Light ? "Light" : "Dark"

                    onClicked: {
                        if (ThemeMode._mode === ThemeMode.System)
                            ThemeMode.set(ThemeMode.Dark);
                        else if (ThemeMode._mode === ThemeMode.Dark)
                            ThemeMode.set(ThemeMode.Light);
                        else
                            ThemeMode.set(ThemeMode.System);
                    }
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columnSpacing: IKSpacing.s12
                columns: 3
                rowSpacing: IKSpacing.s12

                MetricTile {
                    detail: currentSyncModel.hasSync ? currentSyncModel.localPath : "No sync"
                    title: "Current"
                    value: currentSyncModel.hasSync ? `#${currentSyncModel.syncDbId}` : "-"
                }
                MetricTile {
                    detail: currentSyncModel.hasError ? currentSyncModel.latestErrorPath : "No latest error"
                    title: "Errors"
                    value: currentSyncModel.hasError ? String(currentSyncModel.errorCount) : "0"
                }
                UserTile {
                    avatarSource: currentSyncModel.hasSync ? currentSyncModel.userAvatarSource : ""
                    email: currentSyncModel.hasSync ? currentSyncModel.userEmail : "No current sync"
                    name: currentSyncModel.hasSync ? currentSyncModel.userName : "No user"
                }
            }
            RowLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: IKSpacing.s16

                DebugPanel {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 390
                    title: `Syncs (${syncList.count})`

                    ListView {
                        id: syncList

                        anchors.fill: parent
                        clip: true
                        model: syncListModel
                        spacing: IKSpacing.s8

                        ScrollBar.vertical: ScrollBar {
                        }
                        delegate: SyncRow {
                            width: syncList.width

                            onSelectRequested: syncListModel.selectSync(dbId)
                            onUserRequested: availableDriveListModel.selectUser(userDbId)
                        }
                    }
                }
                DebugPanel {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 330
                    title: `Drives (${driveList.count})`

                    ListView {
                        id: driveList

                        anchors.fill: parent
                        clip: true
                        model: driveListModel
                        spacing: IKSpacing.s8

                        ScrollBar.vertical: ScrollBar {
                        }
                        delegate: DriveRow {
                            width: driveList.width

                            onSelectRequested: driveListModel.selectFirstSyncForDrive(dbId)
                            onUserRequested: availableDriveListModel.selectUser(userDbId)
                        }
                    }
                }
                DebugPanel {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    title: `Available drives (${availableDriveList.count})`

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: IKSpacing.s8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: IKSpacing.s8

                            Text {
                                Layout.fillWidth: true
                                color: IKColors.textSecondary
                                elide: Text.ElideRight
                                text: onboardingState.selectedUserDbId > 0 ? `User #${onboardingState.selectedUserDbId}` : "No onboarding user selected"
                            }
                            Button {
                                enabled: onboardingState.selectedUserDbId > 0
                                text: "Clear"

                                onClicked: availableDriveListModel.clearSelectedUser()
                            }
                        }
                        ListView {
                            id: availableDriveList

                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            clip: true
                            model: availableDriveListModel
                            spacing: IKSpacing.s8

                            ScrollBar.vertical: ScrollBar {
                            }
                            delegate: AvailableDriveRow {
                                width: availableDriveList.width

                                onToggleRequested: availableDriveListModel.toggleAvailableDrive(accountId, driveId)
                            }
                        }
                    }
                }
            }
        }
    }

    component AvailableDriveRow: Rectangle {
        id: availableDriveRow

        required property var accountId
        required property var accountName
        required property var alreadyConfigured
        required property var configuredDriveName
        required property var driveId
        required property var hasPendingConfig
        required property var name
        required property var pendingLocalPath
        required property var selected

        signal toggleRequested

        border.color: selected ? IKColors.accentPrimary : IKColors.surfaceTertiary
        border.width: 1
        color: selected ? IKColors.statusLightSecurity : IKColors.surfacePrimary
        height: 98
        opacity: alreadyConfigured ? 0.68 : 1
        radius: 8

        RowLayout {
            anchors.fill: parent
            anchors.margins: IKSpacing.s8
            spacing: IKSpacing.s8

            CheckBox {
                checked: selected
                enabled: !alreadyConfigured

                onClicked: availableDriveRow.toggleRequested()
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: IKSpacing.s4

                Text {
                    Layout.fillWidth: true
                    color: IKColors.textPrimary
                    elide: Text.ElideRight
                    font.weight: Font.DemiBold
                    text: `${name} / drive ${driveId}`
                }
                Text {
                    Layout.fillWidth: true
                    color: IKColors.textSecondary
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    text: `${accountName} / account ${accountId}`
                }
                Text {
                    Layout.fillWidth: true
                    color: alreadyConfigured ? IKColors.statusMediumNeutral : hasPendingConfig ? IKColors.statusMediumSecurity : IKColors.textTertiary
                    elide: Text.ElideMiddle
                    font.pixelSize: 12
                    text: alreadyConfigured ? `Configured as ${configuredDriveName}` : hasPendingConfig ? `Pending ${pendingLocalPath}` : "Not configured"
                }
            }
        }
    }
    component DebugPanel: Rectangle {
        default property alias content: body.data
        required property string title

        border.color: IKColors.surfaceTertiary
        border.width: 1
        color: IKColors.surfaceSecondary
        radius: 8

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: IKSpacing.s12
            spacing: IKSpacing.s12

            Text {
                Layout.fillWidth: true
                color: IKColors.textPrimary
                elide: Text.ElideRight
                font.pixelSize: 15
                font.weight: Font.DemiBold
                text: title
            }
            Item {
                id: body

                Layout.fillHeight: true
                Layout.fillWidth: true
            }
        }
    }
    component DriveRow: Rectangle {
        id: driveRow

        required property var accountName
        required property var current
        required property var dbId
        required property var name
        required property var size
        required property var syncCount
        required property var usedSize
        required property var userDbId
        required property var userEmail

        signal selectRequested
        signal userRequested

        border.color: current ? IKColors.accentPrimary : IKColors.surfaceTertiary
        border.width: 1
        color: current ? IKColors.statusLightSecurity : IKColors.surfacePrimary
        height: 116
        radius: 8

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: IKSpacing.s8
            spacing: IKSpacing.s4

            RowLayout {
                Layout.fillWidth: true
                spacing: IKSpacing.s8

                Text {
                    Layout.fillWidth: true
                    color: IKColors.textPrimary
                    elide: Text.ElideRight
                    font.weight: Font.DemiBold
                    text: `${name} / #${dbId}`
                }
                Text {
                    color: IKColors.textSecondary
                    font.pixelSize: 12
                    text: `${syncCount} sync${syncCount === 1 ? "" : "s"}`
                }
            }
            Text {
                Layout.fillWidth: true
                color: IKColors.textSecondary
                elide: Text.ElideRight
                font.pixelSize: 12
                text: `${accountName} / ${userEmail}`
            }
            Text {
                Layout.fillWidth: true
                color: IKColors.textTertiary
                elide: Text.ElideRight
                font.pixelSize: 12
                text: `${sizeLabel(usedSize)} used / ${sizeLabel(size)} total`
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: IKSpacing.s8

                Button {
                    enabled: syncCount > 0
                    text: "First sync"

                    onClicked: driveRow.selectRequested()
                }
                Button {
                    text: "User drives"

                    onClicked: driveRow.userRequested()
                }
                Item {
                    Layout.fillWidth: true
                }
            }
        }
    }
    component MetricTile: Rectangle {
        required property string detail
        required property string title
        required property string value

        Layout.fillWidth: true
        Layout.preferredHeight: 86
        border.color: IKColors.surfaceTertiary
        border.width: 1
        color: IKColors.surfaceSecondary
        radius: 8

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: IKSpacing.s12
            spacing: IKSpacing.s4

            Text {
                Layout.fillWidth: true
                color: IKColors.textTertiary
                elide: Text.ElideRight
                font.pixelSize: 12
                text: title
            }
            Text {
                Layout.fillWidth: true
                color: IKColors.textPrimary
                elide: Text.ElideRight
                font.pixelSize: 22
                font.weight: Font.DemiBold
                text: value
            }
            Text {
                Layout.fillWidth: true
                color: IKColors.textSecondary
                elide: Text.ElideMiddle
                font.pixelSize: 12
                text: detail
            }
        }
    }
    component UserTile: Rectangle {
        required property string avatarSource
        required property string email
        required property string name

        Layout.fillWidth: true
        Layout.preferredHeight: 86
        border.color: IKColors.surfaceTertiary
        border.width: 1
        color: IKColors.surfaceSecondary
        radius: 8

        RowLayout {
            anchors.fill: parent
            anchors.margins: IKSpacing.s12
            spacing: IKSpacing.s12

            Rectangle {
                Layout.preferredHeight: 48
                Layout.preferredWidth: 48
                border.color: IKColors.statusMediumSecurity
                border.width: 1
                clip: true
                color: IKColors.statusLightSecurity
                radius: 24

                Image {
                    id: avatarImage

                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    source: avatarSource
                    visible: avatarSource.length > 0 && status !== Image.Error
                }
                Text {
                    anchors.centerIn: parent
                    color: IKColors.statusMediumSecurity
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    text: initials(name, email)
                    visible: !avatarImage.visible
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: IKSpacing.s4

                Text {
                    Layout.fillWidth: true
                    color: IKColors.textTertiary
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    text: "User"
                }
                Text {
                    Layout.fillWidth: true
                    color: IKColors.textPrimary
                    elide: Text.ElideRight
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    text: name
                }
                Text {
                    Layout.fillWidth: true
                    color: IKColors.textSecondary
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    text: email
                }
            }
        }
    }
    component SyncRow: Rectangle {
        id: syncRow

        required property var accountName
        required property var dbId
        required property var driveName
        required property var errorCount
        required property var hasError
        required property var localPath
        required property var selected
        required property var targetPath
        required property var userDbId
        required property var userEmail

        signal selectRequested
        signal userRequested

        border.color: selected ? IKColors.accentPrimary : IKColors.surfaceTertiary
        border.width: 1
        color: selected ? IKColors.statusLightSecurity : IKColors.surfacePrimary
        height: 128
        radius: 8

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: IKSpacing.s8
            spacing: IKSpacing.s4

            RowLayout {
                Layout.fillWidth: true
                spacing: IKSpacing.s8

                Text {
                    Layout.fillWidth: true
                    color: IKColors.textPrimary
                    elide: Text.ElideRight
                    font.weight: Font.DemiBold
                    text: `${driveName} / #${dbId}`
                }
                Text {
                    color: hasError ? IKColors.statusStrongWarning : IKColors.statusMediumSuccess
                    font.pixelSize: 12
                    text: errorLabel(hasError, errorCount)
                }
            }
            Text {
                Layout.fillWidth: true
                color: IKColors.textSecondary
                elide: Text.ElideRight
                font.pixelSize: 12
                text: `${accountName} / ${userEmail}`
            }
            Text {
                Layout.fillWidth: true
                color: IKColors.textTertiary
                elide: Text.ElideMiddle
                font.pixelSize: 12
                text: localPath
            }
            Text {
                Layout.fillWidth: true
                color: IKColors.textTertiary
                elide: Text.ElideMiddle
                font.pixelSize: 12
                text: targetPath
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: IKSpacing.s8

                Button {
                    enabled: !selected
                    text: "Select"

                    onClicked: syncRow.selectRequested()
                }
                Button {
                    text: "User drives"

                    onClicked: syncRow.userRequested()
                }
                Item {
                    Layout.fillWidth: true
                }
            }
        }
    }
}
