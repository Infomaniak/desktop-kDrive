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

ScrollView {
    id: root
    required property var controller
    signal releaseInformationRequested(var trigger)
    signal aboutRequested(var trigger)
    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    Column {
        width: root.availableWidth
        padding: IKSettings.pageMargin
        spacing: IKSettings.groupSpacing
        IKLoadingSpinner {
            visible: !root.controller.ready
            width: IKSettings.iconButtonSize
            height: width
            Accessible.name: qsTrId("settingsTitle")
        }
        Text {
            width: parent.width - 2 * IKSettings.pageMargin
            visible: text.length > 0
            text: root.controller.errorText
            color: IKColors.statusStrongWarning
            font.pixelSize: IKFonts.bodySize
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.AlertMessage
        }
        SettingsGroup {
            width: parent.width - 2 * IKSettings.pageMargin
            SettingsRow {
                title: qsTrId("updateSettings")
                description: root.controller.updateText
                IKModalButton {
                    visible: root.controller.updateAvailable
                    text: qsTrId("buttonDownloadManually")
                    external: true
                    implicitHeight: IKSettings.buttonHeight
                    onClicked: root.controller.openDownload()
                }
                SettingsInfoButton {
                    id: releaseInfo
                    onClicked: root.releaseInformationRequested(releaseInfo)
                }
            }
            SettingsRow {
                title: qsTrId("aboutKDrive")
                separator: false
                SettingsInfoButton {
                    id: aboutInfo
                    onClicked: root.aboutRequested(aboutInfo)
                }
            }
        }
        SettingsGroup {
            width: parent.width - 2 * IKSettings.pageMargin
            SettingsRow {
                title: qsTrId("languageSetting")
                IKComboBox {
                    id: languageCombo
                    accessibleName: qsTrId("languageSetting")
                    enabled: root.controller.ready && !root.controller.saving
                    model: root.controller.languages
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: {
                        const options = root.controller.languages;
                        for (let i = 0; i < options.length; ++i) {
                            if (options[i].value === root.controller.language) {
                                return i;
                            }
                        }
                        return 0;
                    }
                    onActivated: root.controller.setLanguage(currentValue)
                }
            }
            SettingsRow {
                title: qsTrId("openKDriveAtStartupSetting")
                IKSwitch {
                    text: qsTrId("openKDriveAtStartupSetting")
                    enabled: root.controller.ready && !root.controller.saving
                    value: root.controller.autoStart
                    onToggleRequested: value => root.controller.setAutoStart(value)
                }
            }
            SettingsRow {
                title: qsTrId("labelNotifications")
                IKSwitch {
                    text: qsTrId("labelNotifications")
                    enabled: root.controller.ready && !root.controller.saving
                    value: root.controller.notificationsEnabled
                    onToggleRequested: value => root.controller.setNotificationsEnabled(value)
                }
            }
            SettingsRow {
                title: qsTrId("moveDeletedFilesToRecycleBinSetting")
                description: qsTrId("moveDeletedFilesToRecycleBinWarning")
                separator: false
                IKSwitch {
                    text: qsTrId("moveDeletedFilesToRecycleBinSetting")
                    enabled: root.controller.ready && !root.controller.saving
                    value: root.controller.moveToTrash
                    onToggleRequested: value => root.controller.setMoveToTrash(value)
                }
                SettingsInfoButton {
                    onClicked: root.controller.openTrashHelp()
                }
            }
        }
        SettingsGroup {
            width: parent.width - 2 * IKSettings.pageMargin
            SettingsRow {
                title: qsTrId("needHelpSetting")
                IKModalButton {
                    text: qsTrId("buttonHelpdesk")
                    role: IKModalButton.Tonal
                    external: true
                    implicitHeight: IKSettings.buttonHeight
                    onClicked: root.controller.openSupport()
                }
            }
            SettingsRow {
                title: qsTrId("feedbackSetting")
                separator: false
                IKModalButton {
                    text: qsTrId("buttonFeedback")
                    role: IKModalButton.Tonal
                    external: true
                    implicitHeight: IKSettings.buttonHeight
                    onClicked: root.controller.openFeedback()
                }
            }
        }
    }
}
