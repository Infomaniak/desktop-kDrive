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
import QtQuick.Controls.Basic
import kDrive.UI

ScrollView {
    id: root

    required property var controller
    readonly property string navigationTitle: qsTrId("networkSettings")
    readonly property bool inputsEnabled: controller.ready && !controller.checking && !controller.saving
    readonly property var proxyTypes: [
        { "label": qsTrId("labelSameAsSystem"), "value": controller.systemProxyType },
        { "label": qsTrId("proxyTypeHTTP"), "value": controller.manualProxyType },
        { "label": qsTrId("proxyTypeNone"), "value": controller.noProxyType }
    ]

    signal connectionFailureRequested(var trigger)

    component ProxyTextField: TextField {
        id: field

        required property string accessibleName

        implicitWidth: IKSettings.comboWidth
        implicitHeight: IKSettings.textFieldHeight
        color: enabled ? IKColors.textPrimary : IKColors.actionDisabled
        placeholderTextColor: IKColors.textTertiary
        font.pixelSize: IKFonts.bodySize
        selectByMouse: true
        leftPadding: IKSpacing.s12
        rightPadding: IKSpacing.s12
        Accessible.name: accessibleName

        background: Rectangle {
            radius: IKRadius.r6
            color: IKColors.surfacePrimary
            border.width: field.activeFocus ? 2 : 1
            border.color: field.activeFocus ? IKColors.accentPrimary : IKColors.settingsDivider
        }
    }

    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    Component.onCompleted: controller.beginEditing()
    // Leaving the page abandons the check, like closing the window: its failure would have no receiver.
    Component.onDestruction: controller.cancelConnectionCheck()

    Connections {
        target: root.controller

        function onProxyConnectionFailureRequested() {
            root.connectionFailureRequested(saveButton);
        }
    }

    Column {
        width: root.availableWidth
        padding: IKSettings.pageMargin

        IKLoadingSpinner {
            visible: !root.controller.ready
            width: IKSettings.iconButtonSize
            height: width
            Accessible.name: qsTrId("networkSettings")
        }

        SettingsGroup {
            width: parent.width - 2 * IKSettings.pageMargin

            SettingsRow {
                title: qsTrId("proxySettings")
                description: qsTrId("proxyConnectionDescription")
                separator: root.controller.manual

                IKComboBox {
                    id: proxyTypeCombo

                    accessibleName: qsTrId("proxySettings")
                    enabled: root.inputsEnabled
                    model: root.proxyTypes
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: {
                        for (let index = 0; index < root.proxyTypes.length; ++index) {
                            if (root.proxyTypes[index].value === root.controller.proxyType) return index;
                        }
                        return 2;
                    }
                    onActivated: root.controller.setProxyType(currentValue)
                }
            }

            SettingsRow {
                visible: root.controller.manual
                title: qsTrId("proxyType")

                ProxyTextField {
                    accessibleName: qsTrId("proxyType")
                    enabled: false
                    text: "HTTP(S)"
                }
            }

            SettingsRow {
                visible: root.controller.manual
                title: qsTrId("proxyHost")

                ProxyTextField {
                    accessibleName: qsTrId("proxyHost")
                    enabled: root.inputsEnabled
                    maximumLength: 200
                    text: root.controller.hostName
                    onTextEdited: root.controller.setHostName(text)
                }
            }

            SettingsRow {
                visible: root.controller.manual
                title: qsTrId("proxyPort")

                ProxyTextField {
                    accessibleName: qsTrId("proxyPort")
                    enabled: root.inputsEnabled
                    maximumLength: 5
                    inputMethodHints: Qt.ImhDigitsOnly
                    text: root.controller.portText
                    validator: RegularExpressionValidator { regularExpression: /^[0-9]*$/ }
                    onTextEdited: root.controller.setPortText(text)
                }
            }

            SettingsRow {
                visible: root.controller.manual
                title: qsTrId("proxyNeedAuth")

                IKSwitch {
                    text: qsTrId("proxyNeedAuth")
                    enabled: root.inputsEnabled
                    value: root.controller.needsAuth
                    onToggleRequested: value => root.controller.setNeedsAuth(value)
                }
            }

            SettingsRow {
                visible: root.controller.manual && root.controller.needsAuth
                title: qsTrId("proxyUser")

                ProxyTextField {
                    accessibleName: qsTrId("proxyUser")
                    enabled: root.inputsEnabled
                    text: root.controller.user
                    onTextEdited: root.controller.setUser(text)
                }
            }

            SettingsRow {
                visible: root.controller.manual && root.controller.needsAuth
                title: qsTrId("proxyPassword")
                separator: false

                ProxyTextField {
                    accessibleName: qsTrId("proxyPassword")
                    enabled: root.inputsEnabled
                    echoMode: TextInput.Password
                    text: root.controller.password
                    onTextEdited: root.controller.setPassword(text)
                }
            }

            Item {
                visible: root.controller.manual
                width: parent.width
                implicitHeight: visible ? saveButton.implicitHeight + 2 * IKSettings.rowPadding : 0

                IKModalButton {
                    id: saveButton

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTrId("buttonSave")
                    actionEnabled: root.controller.valid && !root.controller.checking && !root.controller.saving
                    busy: root.controller.checking || root.controller.saving
                    onClicked: root.controller.saveManual()
                }
            }
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
    }
}
