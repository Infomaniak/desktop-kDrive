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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import kDrive.UI

ScrollView {
    id: root

    required property var controller
    readonly property string navigationTitle: qsTrId("logLevelDebug")
    // Screen of the user action log (see UserActionLog).
    readonly property string screenId: "settings.advanced.debug"

    signal sendLogsRequested(var trigger)

    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    Column {
        width: root.availableWidth
        padding: IKSettings.pageMargin
        spacing: IKSpacing.s16

        Text {
            width: parent.width - 2 * IKSettings.pageMargin
            text: qsTrId("debugLogsSettings")
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.titleSize
            font.weight: IKFonts.emphasized
        }

        Text {
            width: parent.width - 2 * IKSettings.pageMargin
            text: qsTrId("debugLogsDescription")
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.bodySize
            wrapMode: Text.WordWrap
        }

        SettingsGroup {
            width: parent.width - 2 * IKSettings.pageMargin

            SettingsRow {
                title: qsTrId("enableDebugLogsSetting")
                description: qsTrId("enableDebugLogDescription")
                IKSwitch {
                    text: qsTrId("enableDebugLogsSetting")
                    actionId: root.screenId + ".useLog"
                    value: root.controller.useLog
                    enabled: root.controller.ready && !root.controller.saving
                    onToggleRequested: value => root.controller.setUseLog(value)
                }
            }

            SettingsRow {
                title: qsTrId("autoCleanupLogsSetting")
                description: qsTrId("autoCleanupLogsDescription")
                IKSwitch {
                    text: qsTrId("autoCleanupLogsSetting")
                    actionId: root.screenId + ".purgeOldLogs"
                    value: root.controller.purgeOldLogs
                    enabled: root.controller.ready && !root.controller.saving
                    onToggleRequested: value => root.controller.setPurgeOldLogs(value)
                }
            }

            SettingsRow {
                title: qsTrId("extendedLogSetting")
                description: qsTrId("extendedLogDescription")
                SettingsInfoButton {
                    text: qsTrId("extendedLogWarning")
                    actionId: root.screenId + ".extendedLogInfo"
                }
                IKSwitch {
                    text: qsTrId("extendedLogSetting")
                    actionId: root.screenId + ".extendedLog"
                    value: root.controller.extendedLog
                    enabled: root.controller.ready && !root.controller.saving
                    onToggleRequested: value => root.controller.setExtendedLog(value)
                }
            }

            SettingsRow {
                enabled: root.controller.debugLevelEnabled
                title: qsTrId("debugLevelSetting")
                description: qsTrId("debugLevelDescription")
                separator: false
                opacity: enabled ? 1 : 0.55
                IKComboBox {
                    accessibleName: qsTrId("debugLevelSetting")
                    actionId: root.screenId + ".logLevel"
                    enabled: !root.controller.saving
                    model: root.controller.logLevels
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.controller.logLevel
                    onActivated: root.controller.setLogLevel(currentValue)
                }
            }
        }

        IKModalButton {
            text: qsTrId("buttonOpenDebugFolder")
            actionId: root.screenId + ".openFolder"
            role: IKModalButton.Tonal
            onClicked: root.controller.openDebugFolder()
        }

        SettingsGroup {
            width: parent.width - 2 * IKSettings.pageMargin

            SettingsRow {
                title: qsTrId("infomaniakSupport")
                separator: false
                leadingAccessories: [
                    SettingsSupportIcon {}
                ]

                IKLoadingSpinner {
                    Layout.preferredWidth: IKSettings.informationIconSize
                    Layout.preferredHeight: IKSettings.informationIconSize
                    visible: root.controller.uploadInProgress
                    strokeWidth: 2
                    Accessible.role: Accessible.Indicator
                    Accessible.name: root.controller.uploadStatusText
                }

                SettingsInfoButton {
                    readonly property bool failed: root.controller.latestUploadFailed

                    actionId: root.screenId + ".lastUploadInfo"
                    actionDetails: ({ failed: failed })
                    visible: !root.controller.uploadInProgress
                             && (failed || root.controller.lastSuccessfulUploadText.length > 0)
                    text: failed ? qsTrId("logsUploadErrorTooltip")
                                 : qsTrId("logsUploadSuccess") + "\n" + root.controller.lastSuccessfulUploadText
                    iconSource: failed ? "qrc:/assets/main/triangle-alert.svg"
                                       : "qrc:/assets/main/activities/status-synchronized-outline.svg"
                    iconColor: failed ? IKColors.statusStrongWarning : IKColors.statusMediumSuccess
                }

                IKModalButton {
                    id: sendLogsButton
                    text: qsTrId("buttonSendLog")
                    actionId: root.screenId + ".sendLogs"
                    onClicked: root.sendLogsRequested(sendLogsButton)
                }
            }
        }

        Text {
            width: parent.width - 2 * IKSettings.pageMargin
            visible: root.controller.debugErrorText.length > 0
            text: root.controller.debugErrorText
            color: IKColors.statusStrongWarning
            font.pixelSize: IKFonts.bodySize
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.AlertMessage
        }
    }
}
