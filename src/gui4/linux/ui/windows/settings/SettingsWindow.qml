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
import QtQuick.Layouts
import kDrive.UI

IKShadowedWindow {
    id: root
    required property var controller
    visible: false
    transientParent: null
    modality: Qt.NonModal
    title: qsTrId("settingsTitle") + " - kDrive"
    contentWidth: IKSettings.windowWidth
    contentHeight: IKSettings.windowHeight
    minimumContentWidth: IKSettings.minimumWidth
    minimumContentHeight: IKSettings.minimumHeight
    customShadowEnabled: true
    windowTitleVisible: false
    headerBackgroundData: Rectangle {
        width: IKSettings.sidebarWidth
        height: parent.height
        color: IKColors.surfaceSecondary
    }
    headerData: Text {
        anchors.left: parent.left
        anchors.leftMargin: IKSettings.sidebarWidth + IKSettings.pageMargin
        anchors.verticalCenter: parent.verticalCenter
        text: qsTrId("sidebarItemGeneral")
        font.pixelSize: IKFonts.headlineSize
        font.weight: IKFonts.emphasized
        color: IKColors.textPrimary
    }
    RowLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.preferredWidth: IKSettings.sidebarWidth
            Layout.fillHeight: true
            color: IKColors.surfaceSecondary
            Column {
                anchors.fill: parent
                anchors.margins: IKSpacing.s8
                IKSidebarItem {
                    width: parent.width
                    label: qsTrId("sidebarItemGeneral")
                    iconSource: "qrc:/assets/settings/general.svg"
                    selected: true
                    notificationDot: root.controller.updateAvailable
                }
                IKSidebarItem {
                    width: parent.width
                    label: qsTrId("sidebarItemAccounts")
                    iconSource: "qrc:/assets/settings/accounts.svg"
                    enabled: false
                }
                IKSidebarItem {
                    width: parent.width
                    label: qsTrId("sidebarItemAdvanced")
                    iconSource: "qrc:/assets/settings/advanced.svg"
                    enabled: false
                }
            }
        }
        GeneralSettingsView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            controller: root.controller
            onReleaseInformationRequested: trigger => releaseDialog.showFrom(trigger)
            onAboutRequested: trigger => aboutDialog.showFrom(trigger)
        }
    }
    SettingsInformationDialog {
        id: releaseDialog
        controller: root.controller
        scrimInset: root.effectiveShadowMargin
        scrimRadius: root.surfaceRadius
    }
    SettingsInformationDialog {
        id: aboutDialog
        controller: root.controller
        about: true
        scrimInset: root.effectiveShadowMargin
        scrimRadius: root.surfaceRadius
    }
    onClosing: {
        releaseDialog.close();
        aboutDialog.close();
    }
}
