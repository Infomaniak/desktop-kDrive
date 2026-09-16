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
import QtQuick.Effects
import kDrive.UI

Item {
    id: root

    implicitWidth: IKSettings.supportIconSize
    implicitHeight: IKSettings.supportIconSize
    Accessible.ignored: true

    MultiEffect {
        id: badgeEffect

        anchors.fill: parent
        source: Item {
            width: badgeEffect.width
            height: badgeEffect.height

            Rectangle {
                anchors.fill: parent
                radius: IKSettings.supportIconRadius
                color: IKColors.settingsSupportIconSurface
                border.width: IKSettings.supportIconBorderWidth
                border.color: IKColors.settingsSupportIconBorder
            }

            Rectangle {
                anchors.fill: parent
                radius: IKSettings.supportIconRadius
                gradient: Gradient {
                    orientation: Gradient.Vertical

                    GradientStop {
                        position: 0
                        color: IKColors.settingsSupportIconHighlightStart
                    }

                    GradientStop {
                        position: 1
                        color: IKColors.settingsSupportIconHighlightEnd
                    }
                }
            }

            IKTintedIcon {
                anchors.fill: parent
                anchors.margins: IKSettings.supportIconGlyphInset
                source: "qrc:/assets/main/home/headphones.svg"
                color: IKColors.settingsSupportIconGlyph
            }

            Rectangle {
                anchors.fill: parent
                radius: IKSettings.supportIconRadius
                color: IKColors.settingsSupportIconHighlightEnd
                border.width: IKSettings.supportIconBorderWidth
                border.color: IKColors.settingsSupportIconInnerBorder
            }
        }
        shadowEnabled: true
        shadowColor: IKColors.settingsSupportIconShadow
        shadowOpacity: IKSettings.supportIconShadowOpacity
        shadowBlur: IKSettings.supportIconShadowBlur
        blurMax: IKSettings.supportIconShadowBlurMax
        shadowVerticalOffset: IKSettings.supportIconShadowOffsetY
    }
}
