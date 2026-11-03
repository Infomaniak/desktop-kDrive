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
import QtQuick.Layouts
import kDrive.UI

// One error of the errors page, as a card: title, item, description and the button of its action. The variants add
// an information tooltip, a copyable path or the technical details of an unexpected error.
Rectangle {
    id: root

    required property string rowId
    required property string titleId
    required property int titleArgCount
    required property int titleCount
    required property string descriptionId
    required property int descriptionArgCount
    required property string nodeLabelId
    required property string itemName
    required property string itemPath
    required property string itemIconName
    required property bool isDirectory
    required property int variant
    required property string copyablePath
    required property var technicalDetails
    required property string actionLabelId
    required property int actionLabelArgCount
    required property bool actionExternal
    required property bool actionPending

    readonly property string title: ErrorTexts.text(root.titleId, root.titleArgCount, root.nodeLabelId, root.titleCount)
    readonly property string fullDescription: ErrorTexts.text(root.descriptionId, root.descriptionArgCount,
                                                              root.nodeLabelId, 0)
    // The information variant keeps the first line as description and shows the advice that follows in its tooltip.
    readonly property int adviceSeparator: root.variant === SyncErrors.InfoTooltip ? root.fullDescription.indexOf("\n") : -1
    readonly property string description: root.adviceSeparator < 0 ? root.fullDescription
                                                                   : root.fullDescription.slice(0, root.adviceSeparator)
    readonly property string advice: root.adviceSeparator < 0 ? "" : root.fullDescription.slice(root.adviceSeparator + 1)
    property bool detailsExpanded: false
    property bool revealed: false

    // One cell per name, then one per value: the details grid fills its rows in that order. The details arrive as a Qt
    // sequence, which has no `flatMap`.
    function detailCells(details) {
        const cells = [];
        for (let index = 0; index < details.length; ++index) {
            cells.push({
                "text": details[index].label,
                "isValue": false
            });
            cells.push({
                "text": details[index].value,
                "isValue": true
            });
        }
        return cells;
    }

    // Brings the attention to this row: the keyboard focus goes to its action, or to the card itself when it has none or
    // the action is running, and the card is outlined for a moment, since a focus ring alone is hard to notice after a
    // mouse click.
    function reveal() {
        if (actionButton.visible && actionButton.enabled) {
            actionButton.forceActiveFocus();
        } else {
            root.forceActiveFocus();
        }
        root.revealed = true;
        revealTimer.restart();
    }

    signal actionTriggered(string rowId)
    signal copyRequested(string rowId)
    signal technicalDetailsCopyRequested(string rowId)

    implicitHeight: card.implicitHeight + 2 * IKActivities.errorsCardVerticalPadding
    radius: IKActivities.errorsCardRadius
    color: IKColors.errorsCardSurface
    border.width: root.revealed || root.activeFocus ? IKActivities.errorsRevealBorderWidth : 0
    border.color: IKColors.accentPrimary
    Accessible.role: Accessible.ListItem
    Accessible.name: root.title
    Accessible.description: root.description

    Timer {
        id: revealTimer

        interval: IKActivities.errorsRevealDuration
        onTriggered: root.revealed = false
    }

    ColumnLayout {
        id: card

        anchors.fill: parent
        anchors.leftMargin: IKActivities.errorsCardHorizontalPadding
        anchors.rightMargin: IKActivities.errorsCardHorizontalPadding
        anchors.topMargin: IKActivities.errorsCardVerticalPadding
        anchors.bottomMargin: IKActivities.errorsCardVerticalPadding
        spacing: IKActivities.errorsCardContentSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: IKActivities.errorsCardActionSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: IKActivities.errorsCardContentSpacing

                Text {
                    Layout.fillWidth: true
                    text: root.title
                    textFormat: Text.PlainText
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: IKFonts.emphasized
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }

                ErrorItemChip {
                    Layout.maximumWidth: card.width * IKActivities.errorsChipMaximumWidthRatio
                    visible: root.itemName.length > 0
                    name: root.itemName
                    path: root.itemPath
                    iconName: root.itemIconName
                    isDirectory: root.isDirectory
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: IKSpacing.s4

                    Text {
                        Layout.fillWidth: true
                        text: root.description
                        textFormat: Text.PlainText
                        color: IKColors.textSecondary
                        font.pixelSize: IKFonts.subheadlineSize
                        wrapMode: Text.WordWrap
                        Accessible.role: Accessible.StaticText
                        Accessible.name: text
                    }

                    ToolButton {
                        id: infoButton

                        Layout.alignment: Qt.AlignTop
                        implicitWidth: IKActivities.errorsInfoIconSize + 2 * IKSpacing.s4
                        implicitHeight: implicitWidth
                        visible: root.advice.length > 0
                        text: qsTrId("accessibilityMoreInformation")
                        focusPolicy: Qt.StrongFocus
                        hoverEnabled: true
                        display: AbstractButton.IconOnly
                        Accessible.name: text
                        Accessible.description: root.advice

                        contentItem: Item {
                            IKTintedIcon {
                                anchors.centerIn: parent
                                width: IKActivities.errorsInfoIconSize
                                height: width
                                source: "qrc:/assets/settings/information.svg"
                                color: IKColors.textSecondary
                            }
                        }

                        background: Rectangle {
                            radius: width / 2
                            color: infoButton.hovered ? IKColors.surfaceTertiary : "transparent"
                            border.width: infoButton.visualFocus ? 2 : 0
                            border.color: IKColors.accentPrimary
                        }

                        IKToolTip {
                            targetButton: infoButton
                            text: root.advice
                        }
                    }
                }
            }

            IKModalButton {
                id: actionButton

                Layout.alignment: Qt.AlignVCenter
                implicitHeight: IKActivities.errorsActionButtonHeight
                visible: root.actionLabelId.length > 0
                role: IKModalButton.Tonal
                text: ErrorTexts.text(root.actionLabelId, root.actionLabelArgCount, root.nodeLabelId, 0)
                external: root.actionExternal
                busy: root.actionPending
                // Several rows can share a label ("Rename file"): the item tells them apart.
                Accessible.description: root.itemName.length > 0 ? root.itemName : root.title
                onClicked: root.actionTriggered(root.rowId)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: IKActivities.errorsCopyFieldHeight
            visible: root.variant === SyncErrors.CopyablePath && root.copyablePath.length > 0
            radius: IKRadius.r4
            color: IKColors.errorsCopyFieldSurface
            border.width: 1
            border.color: IKColors.errorsCopyFieldBorder

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: IKSpacing.s8
                anchors.rightMargin: IKSpacing.s4
                spacing: IKSpacing.s4

                Text {
                    id: pathLabel

                    Layout.fillWidth: true
                    text: root.copyablePath
                    textFormat: Text.PlainText
                    color: IKColors.textSecondary
                    font.pixelSize: IKFonts.bodySize
                    elide: Text.ElideMiddle

                    HoverHandler {
                        id: pathHover
                    }

                    IKToolTip {
                        text: root.copyablePath
                        showRequested: pathHover.hovered && pathLabel.truncated
                    }
                }

                ErrorCopyButton {
                    text: qsTrId("buttonCopyPath")
                    copiedText: qsTrId("pathCopiedToClipboard")
                    onCopyRequested: root.copyRequested(root.rowId)
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.variant === SyncErrors.Technical && root.technicalDetails.length > 0
            spacing: IKActivities.errorsDetailsLineSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: IKSpacing.s4

                // A full-width disclosure row: the hover surface and the hand cursor tell that it unfolds.
                ToolButton {
                    id: detailsButton

                    Layout.fillWidth: true
                    text: qsTrId("defaultErrorDetailsLabel")
                    focusPolicy: Qt.StrongFocus
                    hoverEnabled: true
                    checkable: true
                    checked: root.detailsExpanded
                    padding: IKActivities.errorsDetailsHeaderPadding
                    Accessible.name: text
                    Accessible.role: Accessible.Button
                    onToggled: root.detailsExpanded = checked

                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }

                    contentItem: RowLayout {
                        spacing: IKSpacing.s4

                        Text {
                            Layout.fillWidth: true
                            text: detailsButton.text
                            textFormat: Text.PlainText
                            color: IKColors.textSecondary
                            font.pixelSize: IKFonts.subheadlineSize
                            font.weight: IKFonts.emphasized
                        }

                        IKTintedIcon {
                            Layout.preferredWidth: IKActivities.errorsChipIconSize
                            Layout.preferredHeight: IKActivities.errorsChipIconSize
                            source: "qrc:/assets/main/chevron-down.svg"
                            color: IKColors.textSecondary
                            // Right when folded, down when unfolded, as on the mockup.
                            rotation: root.detailsExpanded ? 0 : -90

                            Behavior on rotation {
                                NumberAnimation {
                                    duration: IKActivities.errorsDetailsChevronDuration
                                }
                            }
                        }
                    }

                    background: Rectangle {
                        radius: IKRadius.r4
                        color: detailsButton.hovered || detailsButton.down ? IKColors.errorsDetailsHover : "transparent"
                        border.width: detailsButton.visualFocus ? 2 : 0
                        border.color: IKColors.accentPrimary
                    }
                }

                ErrorCopyButton {
                    text: qsTrId("buttonCopyTechnicalDetails")
                    copiedText: qsTrId("technicalDetailsCopiedToClipboard")
                    onCopyRequested: root.technicalDetailsCopyRequested(root.rowId)
                }
            }

            // Field names and values in aligned columns, two pairs per row when the card is wide enough, so that every
            // value starts at the same place. The copy button next to the title sends them to the support.
            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: IKActivities.errorsDetailsHeaderPadding
                visible: root.detailsExpanded
                columns: width >= IKActivities.errorsDetailsTwoPairsMinWidth ? 4 : 2
                columnSpacing: IKActivities.errorsDetailsColumnSpacing
                rowSpacing: IKActivities.errorsDetailsLineSpacing

                Repeater {
                    model: root.detailsExpanded ? root.detailCells(root.technicalDetails) : []

                    Text {
                        required property var modelData

                        Layout.fillWidth: modelData.isValue
                        Layout.alignment: Qt.AlignTop
                        text: modelData.text
                        textFormat: Text.PlainText
                        color: modelData.isValue ? IKColors.textPrimary : IKColors.errorsDetailsLabel
                        font.pixelSize: IKFonts.subheadlineSize
                        font.weight: modelData.isValue ? IKFonts.medium : IKFonts.regular
                        // A long value keeps one line and its end, which tells most (file name, function): the copy
                        // button still sends it whole.
                        elide: modelData.isValue ? Text.ElideLeft : Text.ElideNone
                    }
                }
            }
        }
    }
}
