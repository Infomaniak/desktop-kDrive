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
import QtQuick.Controls.Basic
import QtQuick.Layouts
import kDrive.UI

// One version of a conflict in the version selection dialog: its side, its details and its selection state. The whole
// card is a radio button: a click or Space selects its version; the eye button opens the version instead.
Rectangle {
    id: root

    required property string title
    required property url iconSource
    // Tooltip of the eye button, which opens this version.
    required property string openText
    required property string authorName
    required property string size
    required property string date
    required property bool selected
    required property bool mostRecent
    // While loading, the details are empty and the version cannot be selected.
    required property bool loading

    signal selectRequested
    signal openRequested

    implicitHeight: content.implicitHeight + 2 * IKActivities.errorsConflictCardPadding
    radius: IKActivities.errorsConflictCardRadius
    color: IKColors.errorsConflictCardSurface
    border.width: root.activeFocus ? IKModalTokens.focusBorderWidth : 1
    border.color: {
        if (root.selected || root.activeFocus) {
            return IKColors.errorsConflictCardSelectedBorder;
        }
        return cardHover.hovered && !root.loading ? IKColors.errorsConflictCardHoverBorder : "transparent";
    }
    activeFocusOnTab: true
    Accessible.role: Accessible.RadioButton
    Accessible.name: root.title
    Accessible.checkable: true
    Accessible.checked: root.selected
    Accessible.onPressAction: root.select()
    Keys.onSpacePressed: root.select()
    opacity: root.loading ? IKModalTokens.disabledOpacity : 1

    function select() {
        if (!root.loading) {
            root.selectRequested();
        }
    }

    // Unlike a MouseArea, it keeps reporting the hover while the pointer is over the eye button.
    HoverHandler {
        id: cardHover

        cursorShape: root.loading ? Qt.ArrowCursor : Qt.PointingHandCursor
    }

    // Under the content: the eye button keeps its own clicks, every other part of the card selects the version.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onClicked: root.select()
    }

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: IKActivities.errorsConflictCardPadding
        spacing: IKActivities.errorsConflictCardContentSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: IKActivities.errorsConflictCardTitleSpacing

            IKTintedIcon {
                Layout.preferredWidth: IKActivities.errorsConflictCardIconSize
                Layout.preferredHeight: IKActivities.errorsConflictCardIconSize
                source: root.iconSource
                color: IKColors.errorsConflictCardIcon
            }

            Text {
                Layout.fillWidth: !mostRecentBadge.visible
                text: root.title
                textFormat: Text.PlainText
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.medium
                elide: Text.ElideRight
            }

            Rectangle {
                id: mostRecentBadge

                Layout.fillWidth: false
                visible: root.mostRecent
                implicitWidth: mostRecentText.implicitWidth + 2 * IKActivities.errorsConflictBadgeHorizontalPadding
                implicitHeight: mostRecentText.implicitHeight + 2 * IKActivities.errorsConflictBadgeVerticalPadding
                radius: IKActivities.errorsConflictBadgeRadius
                color: IKColors.errorsConflictMostRecentSurface

                Text {
                    id: mostRecentText

                    anchors.centerIn: parent
                    text: qsTrId("labelMostRecent")
                    textFormat: Text.PlainText
                    color: IKColors.errorsConflictMostRecentText
                    font.pixelSize: IKFonts.subheadlineSize
                }
            }

            Item {
                Layout.fillWidth: mostRecentBadge.visible
            }

            ToolButton {
                id: openButton

                implicitWidth: IKActivities.errorsConflictOpenIconSize + 2 * IKSpacing.s4
                implicitHeight: implicitWidth
                focusPolicy: Qt.StrongFocus
                hoverEnabled: true
                display: AbstractButton.IconOnly
                text: root.openText
                // Tells the two eye buttons apart.
                Accessible.name: text + ", " + root.title
                onClicked: root.openRequested()

                contentItem: Item {
                    IKTintedIcon {
                        anchors.centerIn: parent
                        width: IKActivities.errorsConflictOpenIconSize
                        height: width
                        source: "qrc:/assets/main/errors/eye.svg"
                        color: IKColors.errorsConflictOpenIcon
                    }
                }

                background: Rectangle {
                    radius: IKRadius.r4
                    color: openButton.hovered ? IKColors.surfaceTertiary : "transparent"
                    border.width: openButton.visualFocus ? 2 : 0
                    border.color: IKColors.accentPrimary
                }

                IKToolTip {
                    targetButton: openButton
                    text: openButton.text
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: details.implicitHeight + 2 * IKActivities.errorsConflictInfoPadding
            radius: IKActivities.errorsConflictInfoRadius
            color: IKColors.errorsConflictInfoSurface

            ColumnLayout {
                id: details

                anchors.fill: parent
                anchors.margins: IKActivities.errorsConflictInfoPadding
                spacing: IKActivities.errorsConflictInfoSpacing

                Repeater {
                    model: [
                        {
                            "labelId": "labelAuthor",
                            "value": root.authorName
                        },
                        {
                            "labelId": "labelSize",
                            "value": root.size
                        },
                        {
                            "labelId": "labelDate",
                            "value": root.date
                        }
                    ]

                    delegate: ColumnLayout {
                        id: detail

                        required property var modelData
                        required property int index

                        Layout.fillWidth: true
                        spacing: IKActivities.errorsConflictInfoRowPadding

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: IKSpacing.s8

                            Text {
                                id: detailLabel

                                text: qsTrId(detail.modelData.labelId)
                                textFormat: Text.PlainText
                                color: IKColors.errorsConflictInfoLabel
                                font.pixelSize: IKFonts.subheadlineSize
                                font.weight: IKFonts.medium
                            }

                            Text {
                                Layout.fillWidth: true
                                text: detail.modelData.value
                                textFormat: Text.PlainText
                                color: IKColors.textPrimary
                                font.pixelSize: IKFonts.subheadlineSize
                                font.weight: IKFonts.medium
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                                Accessible.name: detailLabel.text + " " + text
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 1
                            visible: detail.index < 2
                            color: IKColors.errorsConflictInfoDivider
                        }
                    }
                }
            }
        }

        // Selection state, drawn like a button but not one: the whole card takes the click.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: IKActivities.errorsConflictSelectHeight
            radius: IKRadius.r6
            color: root.selected ? IKColors.actionPrimary : IKColors.toolbarControlSurface
            Accessible.ignored: true

            Row {
                anchors.centerIn: parent
                spacing: IKActivities.errorsConflictSelectIconSpacing

                IKTintedIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    width: IKActivities.errorsConflictCardIconSize
                    height: width
                    visible: root.selected
                    source: "qrc:/assets/main/errors/circle-check.svg"
                    color: IKColors.actionOnPrimary
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.selected ? qsTrId("buttonSelected") : qsTrId("buttonSelect")
                    textFormat: Text.PlainText
                    color: root.selected ? IKColors.actionOnPrimary : IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: IKFonts.medium
                }
            }
        }
    }
}
