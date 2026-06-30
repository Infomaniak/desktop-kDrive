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
import QtQuick.Effects
import kDrive.UI

Window {
    id: root

    default property alias contentData: contentHost.data
    property alias headerBackgroundData: windowHeader.backgroundData
    property alias headerData: windowHeader.contentData

    property real contentWidth: 900
    property real contentHeight: 600
    property real minimumContentWidth: 0
    property real minimumContentHeight: 0
    property color surfaceColor: IKColors.surfacePrimary
    property bool customShadowEnabled: false
    readonly property bool customShadowActive: customShadowEnabled
                                                && windowDecorationController.customShadowsSupported
    property bool headerVisible: customShadowActive
    property bool headerOverlaysContent: false
    property bool windowTitleVisible: true
    property real surfaceRadius: customFrameVisible ? IKRadius.r16 : 0
    property bool windowDecorationReady: false

    readonly property real shadowMargin: IKShadows.windowMargin
    readonly property bool customFrameVisible: customShadowActive
                                                && visibility !== Window.Maximized
                                                && visibility !== Window.FullScreen
    readonly property real reservedShadowMargin: customShadowActive ? shadowMargin : 0
    readonly property real effectiveShadowMargin: customFrameVisible ? shadowMargin : 0
    readonly property real reservedHeaderHeight: headerVisible && !headerOverlaysContent ? IKWindow.headerHeight : 0
    readonly property real effectiveHeaderHeight: headerVisible && visibility !== Window.FullScreen
                                                  ? IKWindow.headerHeight
                                                  : 0
    readonly property real surfaceWidth: Math.max(0, width - 2 * effectiveShadowMargin)
    readonly property real surfaceHeight: Math.max(0, height - 2 * effectiveShadowMargin)

    function updateWindowDecoration() {
        if (!windowDecorationReady) {
            return
        }
        windowDecorationController.updateWindowDecoration(root, customShadowActive, effectiveShadowMargin,
                                                          IKWindow.resizeHandleThickness)
    }

    flags: customShadowActive ? Qt.Window | Qt.FramelessWindowHint : Qt.Window
    color: customShadowActive ? "transparent" : surfaceColor
    width: contentWidth + 2 * reservedShadowMargin
    height: contentHeight + reservedHeaderHeight + 2 * reservedShadowMargin
    minimumWidth: minimumContentWidth + 2 * reservedShadowMargin
    minimumHeight: minimumContentHeight + reservedHeaderHeight + 2 * reservedShadowMargin

    onWidthChanged: updateWindowDecoration()
    onHeightChanged: updateWindowDecoration()
    onDevicePixelRatioChanged: updateWindowDecoration()
    onCustomShadowActiveChanged: updateWindowDecoration()
    onEffectiveShadowMarginChanged: updateWindowDecoration()

    Component.onCompleted: {
        windowDecorationReady = true
        updateWindowDecoration()
    }

    Item {
        anchors.fill: parent

        MultiEffect {
            id: windowShadow

            anchors.fill: parent
            visible: root.customFrameVisible
            source: Item {
                width: windowShadow.width
                height: windowShadow.height

                Rectangle {
                    x: root.effectiveShadowMargin
                    y: root.effectiveShadowMargin
                    width: root.surfaceWidth
                    height: root.surfaceHeight
                    radius: root.surfaceRadius
                    color: root.surfaceColor
                }
            }
            shadowEnabled: true
            shadowColor: IKShadows.windowColor
            shadowOpacity: IKShadows.windowOpacity
            shadowBlur: 1
            shadowHorizontalOffset: IKShadows.windowOffsetX
            shadowVerticalOffset: IKShadows.windowOffsetY
            blurMax: IKShadows.windowBlur
        }

        Rectangle {
            id: surface

            x: root.effectiveShadowMargin
            y: root.effectiveShadowMargin
            width: root.surfaceWidth
            height: root.surfaceHeight
            color: root.surfaceColor
            radius: root.surfaceRadius
            layer.enabled: root.customFrameVisible
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: surfaceMask
            }

            IKWindowHeader {
                id: windowHeader

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: root.effectiveHeaderHeight
                visible: root.effectiveHeaderHeight > 0
                targetWindow: root
                backgroundColor: root.surfaceColor
                titleVisible: root.windowTitleVisible
                z: 1
            }

            Item {
                id: contentHost

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: root.headerOverlaysContent || !windowHeader.visible ? parent.top : windowHeader.bottom
                anchors.bottom: parent.bottom
            }
        }

        Rectangle {
            id: surfaceMask

            width: surface.width
            height: surface.height
            radius: root.surfaceRadius
            color: "white"
            visible: false
            layer.enabled: true
        }

        MouseArea {
            x: surface.x - width
            y: surface.y
            width: IKWindow.resizeHandleThickness
            height: surface.height
            visible: root.customFrameVisible
            cursorShape: Qt.SizeHorCursor
            onPressed: root.startSystemResize(Qt.LeftEdge)
        }

        MouseArea {
            x: surface.x + surface.width
            y: surface.y
            width: IKWindow.resizeHandleThickness
            height: surface.height
            visible: root.customFrameVisible
            cursorShape: Qt.SizeHorCursor
            onPressed: root.startSystemResize(Qt.RightEdge)
        }

        MouseArea {
            x: surface.x
            y: surface.y - height
            width: surface.width
            height: IKWindow.resizeHandleThickness
            visible: root.customFrameVisible
            cursorShape: Qt.SizeVerCursor
            onPressed: root.startSystemResize(Qt.TopEdge)
        }

        MouseArea {
            x: surface.x
            y: surface.y + surface.height
            width: surface.width
            height: IKWindow.resizeHandleThickness
            visible: root.customFrameVisible
            cursorShape: Qt.SizeVerCursor
            onPressed: root.startSystemResize(Qt.BottomEdge)
        }

        MouseArea {
            x: surface.x - width
            y: surface.y - height
            width: IKWindow.resizeHandleThickness
            height: IKWindow.resizeHandleThickness
            visible: root.customFrameVisible
            cursorShape: Qt.SizeFDiagCursor
            onPressed: root.startSystemResize(Qt.TopEdge | Qt.LeftEdge)
        }

        MouseArea {
            x: surface.x + surface.width
            y: surface.y - height
            width: IKWindow.resizeHandleThickness
            height: IKWindow.resizeHandleThickness
            visible: root.customFrameVisible
            cursorShape: Qt.SizeBDiagCursor
            onPressed: root.startSystemResize(Qt.TopEdge | Qt.RightEdge)
        }

        MouseArea {
            x: surface.x - width
            y: surface.y + surface.height
            width: IKWindow.resizeHandleThickness
            height: IKWindow.resizeHandleThickness
            visible: root.customFrameVisible
            cursorShape: Qt.SizeBDiagCursor
            onPressed: root.startSystemResize(Qt.BottomEdge | Qt.LeftEdge)
        }

        MouseArea {
            x: surface.x + surface.width
            y: surface.y + surface.height
            width: IKWindow.resizeHandleThickness
            height: IKWindow.resizeHandleThickness
            visible: root.customFrameVisible
            cursorShape: Qt.SizeFDiagCursor
            onPressed: root.startSystemResize(Qt.BottomEdge | Qt.RightEdge)
        }
    }
}
