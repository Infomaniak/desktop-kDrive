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
import QtQuick.Layouts
import kDrive.UI

// One strategy of the grouped choice of the version conflicts. The whole card is a radio button of the strategy group:
// a click selects it, the page moves the selection with the arrow keys.
Rectangle {
    id: root

    required property string title
    required property string description
    required property url iconSource
    required property bool recommended
    required property bool selected

    signal selectRequested

    implicitHeight: content.implicitHeight + 2 * IKActivities.conflictsStrategyPadding
    radius: IKActivities.conflictsStrategyRadius
    color: IKColors.conflictsStrategySurface
    border.width: root.activeFocus ? IKModalTokens.focusBorderWidth : 1
    border.color: {
        if (root.activeFocus) {
            return IKColors.accentPrimary;
        }
        return cardHover.hovered ? IKColors.conflictsStrategyHoverBorder : "transparent";
    }
    Accessible.role: Accessible.RadioButton
    Accessible.name: root.title
    Accessible.description: root.description
    Accessible.checkable: true
    Accessible.checked: root.selected
    Accessible.onPressAction: root.selectRequested()

    HoverHandler {
        id: cardHover

        cursorShape: Qt.PointingHandCursor
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onClicked: root.selectRequested()
    }

    RowLayout {
        id: content

        anchors.fill: parent
        anchors.margins: IKActivities.conflictsStrategyPadding
        spacing: IKActivities.conflictsStrategyContentSpacing

        // Radio indicator, drawn: the card is the control.
        Rectangle {
            Layout.alignment: Qt.AlignTop
            implicitWidth: IKActivities.conflictsRadioSize
            implicitHeight: IKActivities.conflictsRadioSize
            radius: width / 2
            color: root.selected ? IKColors.conflictsRadioOn : IKColors.conflictsRadioOff

            Rectangle {
                anchors.centerIn: parent
                width: IKActivities.conflictsRadioDotSize
                height: width
                radius: width / 2
                visible: root.selected
                color: IKColors.conflictsRadioDot
            }
        }

        Rectangle {
            Layout.alignment: Qt.AlignTop
            implicitWidth: IKActivities.conflictsStrategyIconSize
            implicitHeight: IKActivities.conflictsStrategyIconSize
            radius: IKRadius.r4
            color: IKColors.conflictsStrategyIconSurface

            IKTintedIcon {
                anchors.centerIn: parent
                width: IKActivities.conflictsStrategyGlyphSize
                height: width
                source: root.iconSource
                color: IKColors.conflictsStrategyIcon
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: IKActivities.conflictsStrategyTextSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: IKActivities.conflictsStrategyTitleSpacing

                Text {
                    Layout.fillWidth: !recommendedBadge.visible
                    text: root.title
                    textFormat: Text.PlainText
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    id: recommendedBadge

                    visible: root.recommended
                    implicitWidth: recommendedText.implicitWidth + 2 * IKActivities.errorsConflictBadgeHorizontalPadding
                    implicitHeight: recommendedText.implicitHeight + 2 * IKActivities.errorsConflictBadgeVerticalPadding
                    radius: IKActivities.errorsConflictBadgeRadius
                    color: IKColors.conflictsRecommendedSurface

                    Text {
                        id: recommendedText

                        anchors.centerIn: parent
                        text: qsTrId("labelRecommended")
                        textFormat: Text.PlainText
                        color: IKColors.conflictsRecommendedText
                        font.pixelSize: IKFonts.subheadlineSize
                    }
                }

                Item {
                    Layout.fillWidth: recommendedBadge.visible
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.description
                textFormat: Text.PlainText
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.subheadlineSize
                wrapMode: Text.WordWrap
            }
        }
    }
}
