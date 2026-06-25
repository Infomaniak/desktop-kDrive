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
    readonly property bool loginInProgress: onboardingFlowController.loginInProgress
    readonly property bool waitingForWebAuthentication: onboardingFlowController.waitingForWebAuthentication

    function loginTitleText() {
        if (root.loginFailed) {
            return qsTr("Connection error")
        }
        return qsTr("Welcome to kDrive")
    }

    function loginSubtitleText() {
        if (root.loginFailed) {
            return qsTr("An error has occurred, please try again.")
        }
        return qsTr("The fast, secure private cloud, hosted in Switzerland.")
    }

    Column {
        width: Math.min(IKOnboarding.loginContentMaxWidth, root.width - IKSpacing.s64)
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: root.compact ? IKSpacing.s32 : IKOnboarding.loginContentExpandedLeftMargin
        spacing: root.loginFailed ? IKSpacing.s32 : IKSpacing.s24
        visible: !root.loginInProgress

        Column {
            width: parent.width
            spacing: IKSpacing.s8

            Text {
                width: parent.width
                text: root.loginTitleText()
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.largeTitleSize
                font.weight: Font.Bold
                lineHeightMode: Text.FixedHeight
                lineHeight: IKOnboarding.loginTitleLineHeight
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: root.loginSubtitleText()
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.bodySize
                lineHeightMode: Text.FixedHeight
                lineHeight: IKOnboarding.loginBodyLineHeight
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                visible: !root.loginFailed
                text: qsTr("Log in and keep your documents synchronized on all your devices.")
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.bodySize
                lineHeightMode: Text.FixedHeight
                lineHeight: IKOnboarding.loginBodyLineHeight
                wrapMode: Text.WordWrap
            }
        }

        Row {
            spacing: IKOnboarding.loginButtonSpacing

            Button {
                id: createAccountButton

                enabled: !onboardingFlowController.loginInProgress
                height: IKOnboarding.loginButtonHeight
                text: qsTr("Create an account")
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
                text: qsTr("Login")
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

    }

    Column {
        width: Math.min(IKOnboarding.loginContentMaxWidth, root.width - IKSpacing.s64)
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: root.compact ? IKSpacing.s32 : IKOnboarding.loginContentExpandedLeftMargin
        spacing: IKOnboarding.loginBrowserContentSpacing
        visible: root.loginInProgress

        Text {
            width: parent.width
            text: root.waitingForWebAuthentication
                  ? qsTr("Login from your browser…")
                  : qsTr("Just a few more moments, and we’ll load your account…")
            color: IKColors.textPrimary
            font.pixelSize: IKOnboarding.loginBrowserTitleSize
            font.weight: IKFonts.emphasized
            lineHeightMode: Text.FixedHeight
            lineHeight: IKOnboarding.loginBrowserTitleLineHeight
            wrapMode: Text.WordWrap
        }
    }
}
