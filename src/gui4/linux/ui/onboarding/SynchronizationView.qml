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

    required property var onboardingFlowController

    readonly property bool compact: width < IKOnboarding.completionCompactBreakpointWidth

    Column {
        width: Math.min(IKOnboarding.completionContentMaxWidth, root.width - IKSpacing.s64)
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: root.compact ? IKSpacing.s32 : IKOnboarding.completionContentExpandedLeftMargin
        spacing: root.onboardingFlowController.synchronizationFailed
                 ? IKOnboarding.completionErrorSpacing
                 : IKOnboarding.completionTextSpacing

        Column {
            width: parent.width
            spacing: IKOnboarding.completionTextSpacing

            Text {
                width: parent.width
                text: root.onboardingFlowController.synchronizationFailed
                      ? qsTrId("defaultErrorTitle")
                      : root.onboardingFlowController.title
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.largeTitleSize
                font.weight: Font.Bold
                lineHeightMode: Text.FixedHeight
                lineHeight: IKOnboarding.completionTitleLineHeight
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: root.onboardingFlowController.synchronizationFailed
                      ? qsTrId("unexpectedErrorTeachingTipContent")
                      : qsTrId("onboardingSynchronizationInProgressDescription")
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.bodySize
                lineHeightMode: Text.FixedHeight
                lineHeight: IKOnboarding.completionBodyLineHeight
                wrapMode: Text.WordWrap
            }
        }

        Button {
            id: retryButton

            visible: root.onboardingFlowController.synchronizationFailed
            height: IKOnboarding.completionButtonHeight
            text: qsTrId("buttonRetry")
            onClicked: root.onboardingFlowController.retrySynchronization()

            contentItem: Text {
                text: retryButton.text
                color: IKColors.actionOnPrimary
                font.pixelSize: IKFonts.bodySize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                implicitWidth: IKOnboarding.completionButtonMinWidth
                implicitHeight: IKOnboarding.completionButtonHeight
                radius: IKOnboarding.buttonCornerRadius
                color: IKColors.actionPrimary
            }

            padding: 0
            leftPadding: IKSpacing.s16
            rightPadding: IKSpacing.s16
            topPadding: 0
            bottomPadding: 0
        }
    }
}
