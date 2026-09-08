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

ComboBox {
    id: root
    property string accessibleName: ""
    implicitWidth: Math.max(IKSettings.comboWidth, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: IKSettings.buttonHeight
    leftPadding: IKSpacing.s8
    rightPadding: IKSettings.iconButtonSize
    font.pixelSize: IKFonts.bodySize
    focusPolicy: Qt.StrongFocus
    Accessible.name: accessibleName
    contentItem: Text {
        text: root.displayText
        color: root.enabled ? IKColors.textPrimary : IKColors.actionDisabled
        font: root.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: IKTintedIcon {
        x: root.width - width - IKSpacing.s8
        y: (root.height - height) / 2
        width: IKIconSizes.small
        height: width
        source: "qrc:/assets/main/chevron-down.svg"
        color: root.enabled ? IKColors.textSecondary : IKColors.actionDisabled
    }
    background: Rectangle {
        radius: IKRadius.r6
        color: IKColors.surfacePrimary
        border.width: root.visualFocus ? 2 : 1
        border.color: root.visualFocus ? IKColors.accentPrimary : IKColors.surfaceTertiary
    }
    delegate: ItemDelegate {
        id: optionDelegate
        required property int index
        width: root.width
        text: root.textAt(index)
        font: root.font
        highlighted: root.highlightedIndex === index
        contentItem: Text {
            text: optionDelegate.text
            color: IKColors.textPrimary
            font: root.font
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: optionDelegate.highlighted || optionDelegate.hovered ? IKColors.surfaceTertiary : IKColors.surfacePrimary
        }
    }
    popup: Popup {
        y: root.height
        width: root.width
        padding: 1
        implicitHeight: Math.min(contentItem.implicitHeight + 2, IKSettings.minimumHeight / 2)
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            radius: IKRadius.r6
            color: IKColors.surfacePrimary
            border.color: IKColors.surfaceTertiary
        }
    }
}
