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

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import kDrive.UI

// Background of an upsell card: my kSuite+ (gradient border on the theme surface) or kSuite Pro (always dark, with
// blurred gradient spheres). The owner lays its content on top.
Item {
    id: root

    required property bool kSuitePro
    property real radius: IKMainWindow.upsellCardRadius
    // my kSuite+ only: the ring is drawn over this surface.
    property color surfaceColor: "transparent"

    // kSuite Pro: blurred gradient spheres on a dark surface, clipped to the rounded card.
    Item {
        anchors.fill: parent
        visible: root.kSuitePro
        layer.enabled: root.kSuitePro
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: cardMask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }

        Rectangle {
            anchors.fill: parent
            color: IKColors.upsellKSuiteProSurface
        }
        Image {
            id: gradientSpheres

            anchors.fill: parent
            source: "qrc:/assets/main/upsell/upsell-gradient.svg"
            sourceSize: Qt.size(width * 2, height * 2)
            fillMode: Image.Stretch
            visible: false
        }
        MultiEffect {
            anchors.fill: gradientSpheres
            source: gradientSpheres
            blurEnabled: true
            blur: 1.0
            blurMax: IKMainWindow.upsellGradientBlurMax
        }
    }
    Rectangle {
        id: cardMask

        anchors.fill: parent
        radius: root.radius
        visible: false
        layer.enabled: true
    }

    Rectangle {
        anchors.fill: parent
        visible: !root.kSuitePro
        radius: root.radius
        color: root.surfaceColor
    }

    // my kSuite+: a gradient ring drawn as the even-odd fill of two rounded rectangles.
    Shape {
        anchors.fill: parent
        visible: !root.kSuitePro
        layer.enabled: visible
        layer.samples: 4
        layer.smooth: true

        ShapePath {
            strokeWidth: -1
            fillRule: ShapePath.OddEvenFill
            fillGradient: LinearGradient {
                x1: 0
                y1: 0
                x2: root.width
                y2: root.height

                GradientStop {
                    position: 0.0
                    color: IKColors.upsellGradientLightBlue
                }
                GradientStop {
                    position: 0.25
                    color: IKColors.upsellGradientDarkBlue
                }
                GradientStop {
                    position: 0.5
                    color: IKColors.upsellGradientPurple
                }
                GradientStop {
                    position: 0.75
                    color: IKColors.upsellGradientPink
                }
                GradientStop {
                    position: 1.0
                    color: IKColors.upsellGradientOrange
                }
            }

            PathRectangle {
                width: root.width
                height: root.height
                radius: root.radius
            }
            PathRectangle {
                x: IKMainWindow.upsellCardBorderWidth
                y: IKMainWindow.upsellCardBorderWidth
                width: root.width - 2 * IKMainWindow.upsellCardBorderWidth
                height: root.height - 2 * IKMainWindow.upsellCardBorderWidth
                radius: root.radius - IKMainWindow.upsellCardBorderWidth
            }
        }
    }
}
