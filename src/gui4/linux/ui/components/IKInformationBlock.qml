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

// Information block of the main window: an optional icon tile, a title, a subtitle, a primary action and an optional
// dismiss button below it. The owning view handles the signals; the block itself never reads a controller.
Rectangle {
    id: root

    property string title
    property string subtitle
    property int titleTextFormat: Text.PlainText
    property color surfaceColor: IKColors.informationBlockSurface
    // The icon tile is shown only when a source is set.
    property url iconSource
    property color iconTileColor: IKColors.informationBlockIconTile
    property string primaryActionText
    // Appends the external-link glyph to the primary action. Set it whenever the action opens the web browser.
    property bool primaryActionExternal: false
    // The dismiss button is shown only when a label is set.
    property string dismissText
    // Stable, language-independent id of the primary action in the user action log (see UserActionLog); the dismiss
    // button logs `<actionId>.dismiss`.
    required property string actionId

    readonly property real primaryExternalIconAllowance: primaryActionExternal
                                                         ? primaryExternalIcon.width + IKMainWindow.informationBlockExternalIconSpacing
                                                         : 0

    signal primaryActionTriggered
    signal dismissRequested

    function focusPrimaryAction() {
        primaryButton.forceActiveFocus();
    }

    // The actions set the height (64 px with one button, 80 px with the dismiss button below); the texts elide.
    implicitHeight: Math.max(IKMainWindow.informationBlockMinHeight, actionsColumn.implicitHeight + 2 * IKSpacing.s16)
    radius: IKRadius.r12
    color: root.surfaceColor

    Row {
        id: contentRow

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: IKSpacing.s16
        anchors.rightMargin: IKSpacing.s16
        spacing: IKSpacing.s16

        Row {
            id: leadingRow

            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, contentRow.width - actionsColumn.width - contentRow.spacing)
            spacing: IKSpacing.s8

            Rectangle {
                id: iconTile

                width: IKMainWindow.informationBlockIconTileSize
                height: IKMainWindow.informationBlockIconTileSize
                radius: IKMainWindow.informationBlockIconTileRadius
                color: root.iconTileColor
                visible: root.iconSource.toString() !== ""

                Image {
                    anchors.centerIn: parent
                    width: IKMainWindow.informationBlockIconSize
                    height: IKMainWindow.informationBlockIconSize
                    source: root.iconSource
                    sourceSize.width: Math.ceil(width * Math.max(3, Screen.devicePixelRatio))
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }
            }

            Column {
                width: Math.max(0, leadingRow.width - (iconTile.visible ? iconTile.width + leadingRow.spacing : 0))
                spacing: IKSpacing.s2

                Text {
                    width: parent.width
                    text: root.title
                    textFormat: root.titleTextFormat
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: root.subtitle
                    color: IKColors.textSecondary
                    font.pixelSize: IKFonts.subheadlineSize
                    elide: Text.ElideRight
                }
            }
        }

        Column {
            id: actionsColumn

            anchors.verticalCenter: parent.verticalCenter

            Button {
                id: primaryButton

                implicitHeight: IKMainWindow.informationBlockActionButtonHeight
                text: root.primaryActionText
                visible: root.primaryActionText !== ""
                focusPolicy: Qt.StrongFocus
                onClicked: {
                    UserActionLog.action(root.actionId);
                    root.primaryActionTriggered();
                }

                contentItem: Item {
                    id: primaryContent

                    implicitWidth: primaryLabel.implicitWidth + root.primaryExternalIconAllowance
                    implicitHeight: primaryLabel.implicitHeight

                    Row {
                        anchors.centerIn: parent
                        spacing: root.primaryActionExternal ? IKMainWindow.informationBlockExternalIconSpacing : 0

                        Text {
                            id: primaryLabel

                            // Width is taken from the content Item rather than from the Row, whose implicit width this feeds.
                            width: Math.min(implicitWidth, Math.max(0, primaryContent.width - root.primaryExternalIconAllowance))
                            text: primaryButton.text
                            color: IKColors.actionOnPrimary
                            font.pixelSize: IKFonts.bodySize
                            font.weight: IKFonts.emphasized
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        IKExternalLinkIcon {
                            id: primaryExternalIcon

                            anchors.verticalCenter: parent.verticalCenter
                            visible: root.primaryActionExternal
                            color: IKColors.actionOnPrimary
                        }
                    }
                }

                background: Rectangle {
                    implicitHeight: IKMainWindow.informationBlockActionButtonHeight
                    radius: IKRadius.r6
                    color: IKColors.actionPrimary
                    border.width: primaryButton.visualFocus ? IKLinkTokens.focusRingWidth : 0
                    border.color: IKColors.actionOnPrimary
                }

                padding: 0
                leftPadding: IKSpacing.s16
                rightPadding: IKSpacing.s16
                topPadding: 0
                bottomPadding: 0
            }

            Button {
                id: dismissButton

                anchors.horizontalCenter: parent.horizontalCenter
                implicitHeight: IKMainWindow.informationBlockActionButtonHeight
                text: root.dismissText
                visible: root.dismissText !== ""
                focusPolicy: Qt.StrongFocus
                hoverEnabled: true
                // "Close" alone does not say what is closed: the title tells screen reader users.
                Accessible.description: root.title
                onClicked: {
                    UserActionLog.action(root.actionId + ".dismiss");
                    root.dismissRequested();
                }

                contentItem: Text {
                    text: dismissButton.text
                    color: IKColors.informationBlockDismissForeground
                    font.pixelSize: IKFonts.bodySize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    implicitHeight: IKMainWindow.informationBlockActionButtonHeight
                    radius: IKRadius.r6
                    color: dismissButton.hovered || dismissButton.down ? IKColors.informationBlockDismissHover : "transparent"
                    border.width: dismissButton.visualFocus ? IKLinkTokens.focusRingWidth : 0
                    border.color: IKColors.accentPrimary
                }

                padding: 0
                leftPadding: IKSpacing.s16
                rightPadding: IKSpacing.s16
                topPadding: 0
                bottomPadding: 0
            }
        }
    }
}
