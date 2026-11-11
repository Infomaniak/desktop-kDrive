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
    signal betaProgramRequested(var trigger)
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
                    readonly property bool manual: !root.controller.updateReadyToInstall
                                                   && !root.controller.updateDownloading
                                                   && !root.controller.updateInstallable

                    visible: root.controller.updateAvailable
                    // Ready: the update is installed, the button restarts the application on it. Installable: the
                    // button downloads and installs the update, then restarts.
                    text: root.controller.updateReadyToInstall ? qsTrId("buttonRestart")
                                                               : root.controller.updateDownloading
                                                                 ? qsTrId("buttonDownloadInProgress")
                                                                 : root.controller.updateInstallable
                                                                   ? qsTrId("buttonUpdate")
                                                                   : qsTrId("buttonDownloadManually")
                    external: manual
                    actionEnabled: !root.controller.updateDownloading
                    implicitHeight: IKSettings.buttonHeight
                    onClicked: manual ? root.controller.openDownload() : root.controller.installUpdate()
                }
                SettingsInfoButton {
                    id: releaseInfo
                    onClicked: root.releaseInformationRequested(releaseInfo)
                }
            }
            SettingsRow {
                title: qsTrId("automaticUpdatesSetting")
                // Disabled outside an updatable AppImage, with the reason below the title.
                enabled: root.controller.automaticUpdatesAvailable
                description: root.controller.automaticUpdatesAvailable ? ""
                                                                        : qsTrId("automaticUpdatesUnavailableDescription")
                IKSwitch {
                    text: qsTrId("automaticUpdatesSetting")
                    enabled: root.controller.ready && !root.controller.saving
                    // Off when unavailable: the stored parameter has no effect outside an updatable AppImage.
                    value: root.controller.automaticUpdatesAvailable && root.controller.automaticUpdates
                    onToggleRequested: value => root.controller.setAutomaticUpdates(value)
                }
            }
            SettingsRow {
                title: qsTrId("releaseChannelBeta")
                separator: false
                Text {
                    text: root.controller.betaProgramStatus
                    color: IKColors.textSecondary
                    font.pixelSize: IKFonts.bodySize
                }
                SettingsInfoButton {
                    id: betaProgramInfo
                    enabled: root.controller.ready
                    onClicked: root.betaProgramRequested(betaProgramInfo)
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
                SettingsInfoButton {
                    onClicked: root.controller.openTrashHelp()
                }
                IKSwitch {
                    text: qsTrId("moveDeletedFilesToRecycleBinSetting")
                    enabled: root.controller.ready && !root.controller.saving
                    value: root.controller.moveToTrash
                    onToggleRequested: value => root.controller.setMoveToTrash(value)
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
        SettingsGroup {
            width: parent.width - 2 * IKSettings.pageMargin
            SettingsRow {
                title: qsTrId("aboutKDrive")
                separator: false
                SettingsInfoButton {
                    id: aboutInfo
                    onClicked: root.aboutRequested(aboutInfo)
                }
            }
        }
    }
}
