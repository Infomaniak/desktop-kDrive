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
import kDrive.UI

Button {
    id: root

    enum Role {
        Primary,
        Secondary,
        Destructive,
        Tonal
    }

    property int role: IKModalButton.Primary
    property bool actionEnabled: true
    property bool busy: false

    readonly property color foregroundColor: {
        if (!actionEnabled && !busy) {
            return IKColors.actionDisabled
        }
        if (role === IKModalButton.Tonal) {
            return IKColors.actionOnTonal
        }
        if (role === IKModalButton.Secondary) {
            return IKColors.actionPrimary
        }
        return role === IKModalButton.Destructive ? IKColors.actionOnDestructive : IKColors.actionOnPrimary
    }
    readonly property color focusBorderColor: {
        if (role === IKModalButton.Tonal) {
            return IKColors.actionOnTonal
        }
        if (role === IKModalButton.Secondary) {
            return IKColors.accentPrimary
        }
        return role === IKModalButton.Destructive ? IKColors.actionOnDestructive : IKColors.actionOnPrimary
    }

    enabled: actionEnabled && !busy
    implicitWidth: Math.max(IKModalTokens.buttonMinimumWidth,
                            buttonText.implicitWidth + 2 * IKModalTokens.buttonHorizontalPadding)
    implicitHeight: IKModalTokens.buttonHeight
    padding: 0
    leftPadding: IKModalTokens.buttonHorizontalPadding
    rightPadding: IKModalTokens.buttonHorizontalPadding
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Button
    Accessible.name: text
    hoverEnabled: true
    opacity: actionEnabled || busy ? 1 : IKModalTokens.disabledOpacity

    contentItem: Item {
        Text {
            id: buttonText

            anchors.fill: parent
            visible: !root.busy
            text: root.text
            color: root.foregroundColor
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.emphasized
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        IKLoadingSpinner {
            anchors.centerIn: parent
            visible: root.busy
            width: IKModalTokens.buttonSpinnerSize
            height: IKModalTokens.buttonSpinnerSize
            strokeWidth: 2
            color: root.foregroundColor
        }
    }

    background: Rectangle {
        radius: IKRadius.r6
        color: {
            if (root.role === IKModalButton.Secondary) {
                return root.hovered || root.down ? IKColors.modalSecondaryActionHover : "transparent"
            }
            if (root.role === IKModalButton.Tonal) {
                return IKColors.actionTonalSurface
            }
            if (root.role === IKModalButton.Destructive) {
                return root.down ? IKColors.actionDestructivePressed : IKColors.actionDestructive
            }
            return IKColors.actionPrimary
        }
        opacity: root.down ? IKModalTokens.pressedOpacity : root.hovered ? IKModalTokens.hoverOpacity : 1
        border.width: root.visualFocus ? IKModalTokens.focusBorderWidth
                                             : root.role === IKModalButton.Secondary ? IKModalTokens.borderWidth : 0
        border.color: root.visualFocus ? root.focusBorderColor : IKColors.modalBorder
    }
}
