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
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Shapes
import kDrive.UI

// Upsell card of the main sidebar: my kSuite+ (gradient border, follows the theme) or kSuite Pro (always dark).
Item {
    id: root

    required property int offer
    readonly property bool kSuitePro: offer === MainSidebarController.KSuitePro
    readonly property color foregroundColor: kSuitePro ? IKColors.upsellKSuiteProText : IKColors.upsellMyKSuiteText

    signal upgradeRequested

    implicitHeight: content.implicitHeight + 2 * IKMainWindow.upsellCardPadding

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
        radius: IKMainWindow.upsellCardRadius
        visible: false
        layer.enabled: true
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
                radius: IKMainWindow.upsellCardRadius
            }
            PathRectangle {
                x: IKMainWindow.upsellCardBorderWidth
                y: IKMainWindow.upsellCardBorderWidth
                width: root.width - 2 * IKMainWindow.upsellCardBorderWidth
                height: root.height - 2 * IKMainWindow.upsellCardBorderWidth
                radius: IKMainWindow.upsellCardRadius - IKMainWindow.upsellCardBorderWidth
            }
        }
    }

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: IKMainWindow.upsellCardPadding
        spacing: 0

        Image {
            source: {
                if (root.kSuitePro) {
                    return "qrc:/assets/main/upsell/ksuite-pro-logo.svg";
                }
                return IKColors.darkMode ? "qrc:/assets/main/upsell/ksuite-plus-logo-dark.svg"
                                         : "qrc:/assets/main/upsell/ksuite-plus-logo.svg";
            }
            sourceSize.height: 3 * IKMainWindow.upsellLogoHeight
            fillMode: Image.PreserveAspectFit
            Layout.preferredHeight: IKMainWindow.upsellLogoHeight
            Layout.preferredWidth: IKMainWindow.upsellLogoHeight * implicitWidth / Math.max(1, implicitHeight)
            Accessible.role: Accessible.Graphic
            Accessible.name: root.kSuitePro ? "kSuite Pro" : "my kSuite+"
        }
        Text {
            id: description

            Layout.fillWidth: true
            Layout.maximumWidth: IKMainWindow.upsellDescriptionMaxWidth
            Layout.topMargin: IKMainWindow.upsellCardLogoSpacing
            text: root.kSuitePro ? qsTrId("ksuiteProBannerDescription") : qsTrId("myksuitePlusBannerDescription")
            color: root.foregroundColor
            font.pixelSize: IKFonts.bodySize
            wrapMode: Text.Wrap
        }
        // Compact push button of the design: 22 px high, regular weight.
        Button {
            id: upgradeButton

            readonly property color labelColor: root.kSuitePro ? IKColors.upsellKSuiteProText : IKColors.upsellMyKSuiteText

            Layout.topMargin: IKMainWindow.upsellCardActionSpacing
            // A long translation elides instead of widening the card content.
            Layout.maximumWidth: content.width
            implicitWidth: upgradeLabel.implicitWidth + IKMainWindow.informationBlockExternalIconSpacing
                           + upgradeExternalIcon.width + 2 * IKMainWindow.upsellButtonHorizontalPadding
            implicitHeight: IKMainWindow.upsellButtonHeight
            padding: 0
            leftPadding: IKMainWindow.upsellButtonHorizontalPadding
            rightPadding: IKMainWindow.upsellButtonHorizontalPadding
            text: qsTrId("buttonUpgradeSubscriptionShort")
            focusPolicy: Qt.StrongFocus
            hoverEnabled: true
            Accessible.role: Accessible.Button
            Accessible.name: text
            // "Upgrade" alone does not name the offer.
            Accessible.description: description.text

            onClicked: root.upgradeRequested()

            contentItem: Item {
                id: upgradeContent

                Row {
                    anchors.centerIn: parent
                    spacing: IKMainWindow.informationBlockExternalIconSpacing

                    Text {
                        id: upgradeLabel

                        // Width is taken from the content Item rather than from the Row, whose implicit width this feeds.
                        width: Math.min(implicitWidth, Math.max(0, upgradeContent.width
                                                                - IKMainWindow.informationBlockExternalIconSpacing
                                                                - upgradeExternalIcon.width))
                        text: upgradeButton.text
                        color: upgradeButton.labelColor
                        font.pixelSize: IKFonts.bodySize
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    IKExternalLinkIcon {
                        id: upgradeExternalIcon

                        anchors.verticalCenter: parent.verticalCenter
                        color: upgradeButton.labelColor
                    }
                }
            }

            background: Rectangle {
                radius: IKMainWindow.upsellButtonRadius
                color: root.kSuitePro ? IKColors.upsellKSuiteProButtonSurface : IKColors.upsellMyKSuiteButtonSurface
                opacity: upgradeButton.down ? IKModalTokens.pressedOpacity
                                            : upgradeButton.hovered ? IKModalTokens.hoverOpacity : 1
                border.width: upgradeButton.visualFocus ? IKLinkTokens.focusRingWidth
                                                        : root.kSuitePro ? 0 : IKMainWindow.upsellButtonBorderWidth
                border.color: upgradeButton.visualFocus ? IKColors.accentPrimary : IKColors.upsellMyKSuiteButtonBorder
            }
        }
    }
}
