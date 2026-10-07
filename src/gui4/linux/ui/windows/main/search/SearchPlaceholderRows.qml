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
import kDrive.UI

// Skeleton rows shown while the first page of a query loads.
Column {
    id: root

    Accessible.role: Accessible.StaticText
    Accessible.name: qsTrId("accessibilitySearching")

    Repeater {
        model: IKSearch.placeholderRowCount

        Item {
            width: root.width
            height: placeholderContent.height + 2 * IKSearch.rowPadding

            Item {
                id: placeholderContent

                x: IKSearch.rowPadding
                y: IKSearch.rowPadding
                width: parent.width - 2 * IKSearch.rowPadding
                height: IKSearch.rowTextLineHeight + IKSearch.rowSubtitleLineHeight

                Rectangle {
                    id: iconBar

                    y: (IKSearch.rowTextLineHeight - height) / 2
                    width: IKSearch.rowFileIconSize
                    height: IKSearch.rowFileIconSize
                    radius: IKSearch.placeholderBarRadius
                    color: IKColors.surfaceSecondary
                }

                Rectangle {
                    x: iconBar.width + IKSearch.rowSpacing
                    y: (IKSearch.rowTextLineHeight - height) / 2
                    width: (parent.width - x) * IKSearch.placeholderNameWidthRatio
                    height: IKFonts.title3Size * IKSearch.placeholderTextBarHeightRatio
                    radius: IKSearch.placeholderBarRadius
                    color: IKColors.surfaceSecondary
                }

                Rectangle {
                    x: iconBar.width + IKSearch.rowSpacing
                    y: IKSearch.rowTextLineHeight + (IKSearch.rowSubtitleLineHeight - height) / 2
                    width: (parent.width - x) * IKSearch.placeholderSubtitleWidthRatio
                    height: IKFonts.subheadlineSize * IKSearch.placeholderTextBarHeightRatio
                    radius: IKSearch.placeholderBarRadius
                    color: IKColors.surfaceSecondary
                }
            }
        }
    }

    SequentialAnimation on opacity {
        running: root.visible
        loops: Animation.Infinite

        NumberAnimation {
            from: 1
            to: IKSearch.placeholderMinimumOpacity
            duration: IKSearch.placeholderPulseDuration
            easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            from: IKSearch.placeholderMinimumOpacity
            to: 1
            duration: IKSearch.placeholderPulseDuration
            easing.type: Easing.InOutQuad
        }
    }
}
