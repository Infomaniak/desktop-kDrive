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

    readonly property bool compact: width < 520

    Column {
        width: Math.min(378, root.width - 64)
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: root.compact ? 32 : 80
        spacing: 24

        Column {
            width: parent.width
            spacing: 8

            Text {
                width: parent.width
                text: "Bienvenue dans kDrive !"
                color: IKColors.textPrimary
                font.pixelSize: 26
                font.weight: Font.Bold
                lineHeightMode: Text.FixedHeight
                lineHeight: 32
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: "Le cloud privé, rapide et sécurisé, hébergé en Suisse."
                color: IKColors.textSecondary
                font.pixelSize: 13
                lineHeightMode: Text.FixedHeight
                lineHeight: 16
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: "Connectez-vous et gardez vos documents synchronisés\nsur tous vos appareils."
                color: IKColors.textSecondary
                font.pixelSize: 13
                lineHeightMode: Text.FixedHeight
                lineHeight: 16
                wrapMode: Text.WordWrap
            }
        }

        Row {
            spacing: 10

            Button {
                id: createAccountButton

                enabled: !onboardingFlowController.loginInProgress
                height: 24
                text: "Créer un compte"
                onClicked: onboardingFlowController.requestAccountCreation()

                contentItem: Text {
                    text: createAccountButton.text
                    color: createAccountButton.enabled ? IKColors.actionPrimary : IKColors.actionDisabled
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    implicitWidth: 128
                    implicitHeight: 24
                    radius: 6
                    color: "transparent"
                }

                padding: 0
                leftPadding: 16
                rightPadding: 16
                topPadding: 0
                bottomPadding: 0
            }

            Button {
                id: loginButton

                enabled: !onboardingFlowController.loginInProgress
                height: 24
                text: "Se connecter"
                onClicked: onboardingFlowController.requestLogin()

                contentItem: Text {
                    text: loginButton.text
                    color: loginButton.enabled ? IKColors.actionOnPrimary : IKColors.actionDisabled
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    implicitWidth: 105
                    implicitHeight: 24
                    radius: 6
                    color: loginButton.enabled ? IKColors.actionPrimary : IKColors.actionDisabled
                }

                padding: 0
                leftPadding: 16
                rightPadding: 16
                topPadding: 0
                bottomPadding: 0
            }
        }

        Text {
            width: parent.width
            visible: onboardingFlowController.loginStatusText.length > 0
            text: onboardingFlowController.loginStatusText
            color: onboardingFlowController.loginFailed ? IKColors.statusStrongWarning : IKColors.textTertiary
            font.pixelSize: 13
            lineHeightMode: Text.FixedHeight
            lineHeight: 16
            wrapMode: Text.WordWrap
        }
    }
}
