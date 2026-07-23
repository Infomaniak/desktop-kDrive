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
import QtQuick.Effects
import kDrive.UI

// Presents a drive or synchronization selector entry with independent selection, status, and interaction states.
Button {
    id: root

    property int entryType: SyncSelectorModel.ClassicSync
    property string title: ""
    property string subtitle: ""
    property color driveColor: IKColors.driveDefaultColor
    property int errorCount: 0
    property bool warning: false
    property bool selected: false
    property bool interactive: true
    property bool showSurface: true
    property bool showChevron: false
    readonly property bool advancedSync: entryType === SyncSelectorModel.AdvancedSync
    readonly property bool hasSubtitle: subtitle.length > 0
    readonly property string accessibleStatusDescription: {
        const descriptions = []
        if (warning) {
            descriptions.push(qsTrId("logLevelWarning"))
        }
        if (errorCount > 0) {
            descriptions.push(qsTrId("informationBlockSynchroErrorTitle", errorCount)
                              .replace("<b>", "").replace("</b>", ""))
        }
        return descriptions.join(". ")
    }
    readonly property string truncatedTextUnderPointer: {
        if (!pointerArea.containsMouse) {
            return ""
        }
        if (titleText.truncated && textContainsPointer(titleText)) {
            return title
        }
        if (subtitleText.truncated && textContainsPointer(subtitleText)) {
            return subtitle
        }
        return ""
    }
    readonly property string truncatedTextUnderFocus: {
        if (!activeFocus) {
            return ""
        }
        if (titleText.truncated && subtitleText.truncated) {
            return text
        }
        if (titleText.truncated) {
            return title
        }
        if (subtitleText.truncated) {
            return subtitle
        }
        return ""
    }
    readonly property string truncatedText: truncatedTextUnderPointer.length > 0
                                                  ? truncatedTextUnderPointer
                                                  : truncatedTextUnderFocus
    signal triggered

    function textContainsPointer(textItem) {
        const point = pointerArea.mapToItem(textItem, pointerArea.mouseX, pointerArea.mouseY)
        return point.x >= 0 && point.x <= textItem.width && point.y >= 0 && point.y <= textItem.height
    }

    implicitHeight: hasSubtitle ? IKMainWindow.syncSelectorAdvancedHeight : IKMainWindow.syncSelectorHeight
    padding: 0
    enabled: interactive
    focusPolicy: interactive ? Qt.StrongFocus : Qt.NoFocus
    hoverEnabled: interactive
    text: hasSubtitle ? title + ", " + subtitle : title
    Accessible.description: accessibleStatusDescription
    onClicked: triggered()

    background: Rectangle {
        radius: IKRadius.r4
        color: root.selected || root.down || root.hovered ? IKColors.surfaceTertiary
                                                          : root.showSurface ? IKColors.surfacePrimary : "transparent"
        border.width: root.visualFocus ? 2 : 0
        border.color: IKColors.accentPrimary
    }

    contentItem: Item {
        Item {
            id: leadingIcon

            anchors.left: parent.left
            anchors.leftMargin: IKSpacing.s8
            anchors.verticalCenter: parent.verticalCenter
            width: IKMainWindow.syncSelectorIconSize
            height: IKMainWindow.syncSelectorIconSize

            IKDriveIcon {
                anchors.fill: parent
                visible: !root.advancedSync
                driveColor: root.driveColor
            }

            Image {
                anchors.fill: parent
                visible: root.advancedSync
                source: "qrc:/assets/main/folder.svg"
                sourceSize.width: width
                sourceSize.height: height
                layer.enabled: visible
                layer.effect: MultiEffect {
                    colorization: 1
                    colorizationColor: root.driveColor
                }
            }
        }

        Column {
            id: textColumn

            anchors.left: leadingIcon.right
            anchors.leftMargin: IKSpacing.s8
            anchors.right: accessories.left
            anchors.rightMargin: IKSpacing.s8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            Text {
                id: titleText

                width: parent.width
                text: root.title
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.regular
                elide: Text.ElideRight
            }

            Text {
                id: subtitleText

                width: parent.width
                visible: root.hasSubtitle
                text: root.subtitle
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.subheadlineSize
                font.weight: IKFonts.regular
                elide: Text.ElideRight
            }
        }

        Row {
            id: accessories

            anchors.right: parent.right
            anchors.rightMargin: IKSpacing.s8
            anchors.verticalCenter: parent.verticalCenter
            spacing: IKSpacing.s8

            Image {
                visible: root.warning
                width: visible ? IKMainWindow.syncSelectorStatusIconSize : 0
                height: IKMainWindow.syncSelectorStatusIconSize
                source: "qrc:/assets/main/triangle-alert.svg"
                sourceSize.width: width
                sourceSize.height: height
                layer.enabled: visible
                layer.effect: MultiEffect {
                    colorization: 1
                    colorizationColor: IKColors.statusMediumWarning
                }
            }

            IKBadge {
                dot: root.errorCount > 0
            }

            Image {
                visible: root.showChevron
                width: visible ? IKIconSizes.small : 0
                height: IKIconSizes.small
                source: "qrc:/assets/main/chevron-down.svg"
                sourceSize.width: width
                sourceSize.height: height
                layer.enabled: visible
                layer.effect: MultiEffect {
                    colorization: 1
                    colorizationColor: IKColors.textSecondary
                }
            }
        }
    }

    MouseArea {
        id: pointerArea

        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }

    HoverHandler {
        cursorShape: root.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    ToolTip {
        id: truncatedTextTooltip

        visible: root.truncatedText.length > 0
        delay: IKMainWindow.syncSelectorTooltipDelay
        timeout: -1
        text: root.truncatedText
        padding: IKMainWindow.syncSelectorTooltipPadding

        contentItem: Text {
            width: Math.min(implicitWidth, IKMainWindow.syncSelectorTooltipMaxWidth)
            text: truncatedTextTooltip.text
            color: IKColors.tooltipText
            font.pixelSize: IKFonts.bodySize
            wrapMode: Text.WordWrap
        }

        background: Rectangle {
            radius: IKMainWindow.syncSelectorTooltipRadius
            color: IKColors.tooltipSurface
        }
    }
}
