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

    readonly property bool compact: width < IKOnboarding.loginCompactBreakpointWidth
    readonly property bool loginFailed: onboardingFlowController.loginFailed
    readonly property bool loginSucceeded: onboardingFlowController.loginSucceeded
    readonly property bool waitingForWebAuthentication: onboardingFlowController.waitingForWebAuthentication

    Column {
        width: Math.min(IKOnboarding.loginContentMaxWidth, root.width - IKSpacing.s64)
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: root.compact ? IKSpacing.s32 : IKOnboarding.loginContentExpandedLeftMargin
        spacing: root.loginFailed ? IKSpacing.s32 : IKSpacing.s24
        visible: !root.waitingForWebAuthentication

        Column {
            width: parent.width
            spacing: IKSpacing.s8

            Text {
                width: parent.width
                text: root.loginFailed ? qsTr("Erreur de connexion")
                                       : root.loginSucceeded ? qsTr("Connexion réussie !") : qsTr("Bienvenue dans kDrive !")
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.largeTitleSize
                font.weight: Font.Bold
                lineHeightMode: Text.FixedHeight
                lineHeight: IKOnboarding.loginTitleLineHeight
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: root.loginFailed ? qsTr("Une erreur est survenue, veuillez réessayer.")
                                       : root.loginSucceeded ? qsTr("Vous allez passer à l’étape suivante automatiquement.")
                                       : qsTr("Le cloud privé, rapide et sécurisé, hébergé en Suisse.")
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.bodySize
                lineHeightMode: Text.FixedHeight
                lineHeight: IKOnboarding.loginBodyLineHeight
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                visible: !root.loginFailed && !root.loginSucceeded
                text: qsTr("Connectez-vous et gardez vos documents synchronisés\nsur tous vos appareils.")
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.bodySize
                lineHeightMode: Text.FixedHeight
                lineHeight: IKOnboarding.loginBodyLineHeight
                wrapMode: Text.WordWrap
            }
        }

        Row {
            spacing: IKOnboarding.loginButtonSpacing
            visible: !root.loginSucceeded

            Button {
                id: createAccountButton

                enabled: !onboardingFlowController.loginInProgress
                height: IKOnboarding.loginButtonHeight
                text: qsTr("Créer un compte")
                onClicked: onboardingFlowController.requestAccountCreation()

                contentItem: Text {
                    text: createAccountButton.text
                    color: createAccountButton.enabled ? IKColors.actionPrimary : IKColors.actionDisabled
                    font.pixelSize: IKFonts.bodySize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    implicitWidth: IKOnboarding.loginCreateAccountButtonMinWidth
                    implicitHeight: IKOnboarding.loginButtonHeight
                    radius: IKOnboarding.loginButtonCornerRadius
                    color: "transparent"
                }

                padding: 0
                leftPadding: IKSpacing.s16
                rightPadding: IKSpacing.s16
                topPadding: 0
                bottomPadding: 0
            }

            Button {
                id: loginButton

                enabled: !onboardingFlowController.loginInProgress
                height: IKOnboarding.loginButtonHeight
                text: qsTr("Se connecter")
                onClicked: onboardingFlowController.requestLogin()

                contentItem: Text {
                    text: loginButton.text
                    color: loginButton.enabled ? IKColors.actionOnPrimary : IKColors.actionDisabled
                    font.pixelSize: IKFonts.bodySize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    implicitWidth: IKOnboarding.loginButtonMinWidth
                    implicitHeight: IKOnboarding.loginButtonHeight
                    radius: IKOnboarding.loginButtonCornerRadius
                    color: loginButton.enabled ? IKColors.actionPrimary : IKColors.actionDisabled
                }

                padding: 0
                leftPadding: IKSpacing.s16
                rightPadding: IKSpacing.s16
                topPadding: 0
                bottomPadding: 0
            }
        }

        Text {
            width: parent.width
            visible: !root.loginFailed && onboardingFlowController.loginStatusText.length > 0
            text: onboardingFlowController.loginStatusText
            color: IKColors.textTertiary
            font.pixelSize: IKFonts.bodySize
            lineHeightMode: Text.FixedHeight
            lineHeight: IKOnboarding.loginBodyLineHeight
            wrapMode: Text.WordWrap
        }
    }

    Column {
        width: Math.min(IKOnboarding.loginContentMaxWidth, root.width - IKSpacing.s64)
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: root.compact ? IKSpacing.s32 : IKOnboarding.loginContentExpandedLeftMargin
        spacing: IKOnboarding.loginBrowserContentSpacing
        visible: root.waitingForWebAuthentication

        Column {
            width: parent.width
            spacing: IKOnboarding.loginBrowserTextSpacing

            Text {
                width: parent.width
                text: qsTr("Connectez-vous\ndans votre navigateur")
                color: IKColors.textPrimary
                font.pixelSize: IKOnboarding.loginBrowserTitleSize
                font.weight: IKFonts.emphasized
                lineHeightMode: Text.FixedHeight
                lineHeight: IKOnboarding.loginBrowserTitleLineHeight
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: qsTr("Votre navigateur devrait s’ouvrir automatiquement\npour finaliser la connexion. Une fois connecté,\nvous reviendrez automatiquement dans kDrive.")
                color: IKColors.textSecondary
                font.pixelSize: IKOnboarding.loginBrowserBodySize
                lineHeightMode: Text.FixedHeight
                lineHeight: IKOnboarding.loginBrowserBodyLineHeight
                wrapMode: Text.WordWrap
            }
        }

        Button {
            id: openLoginPageButton

            height: IKOnboarding.loginBrowserButtonHeight
            text: qsTr("Ouvrir la page de connexion")
            onClicked: onboardingFlowController.requestLogin()

            contentItem: Text {
                text: openLoginPageButton.text
                color: IKColors.actionOnPrimary
                font.pixelSize: IKOnboarding.loginBrowserBodySize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                implicitWidth: IKOnboarding.loginBrowserButtonMinWidth
                implicitHeight: IKOnboarding.loginBrowserButtonHeight
                radius: IKRadius.r4
                color: IKColors.actionPrimary
            }

            padding: 0
            leftPadding: IKSpacing.s12
            rightPadding: IKSpacing.s12
            topPadding: 0
            bottomPadding: 0
        }
    }
}
