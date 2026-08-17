/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

import QtQuick
import QtQuick.Controls
import kDrive.UI

ToolTip {
    id: root

    property real maximumTextWidth: IKMainWindow.syncSelectorTooltipMaxWidth
    property color foregroundColor: IKColors.tooltipText
    property color surfaceColor: IKColors.tooltipSurface
    property real textLineHeight: 0

    delay: IKMainWindow.tooltipDelay
    timeout: -1
    padding: IKMainWindow.syncSelectorTooltipPadding

    contentItem: Text {
        width: Math.min(implicitWidth, root.maximumTextWidth)
        text: root.text
        color: root.foregroundColor
        font.pixelSize: IKFonts.bodySize
        lineHeightMode: root.textLineHeight > 0 ? Text.FixedHeight : Text.ProportionalHeight
        lineHeight: root.textLineHeight > 0 ? root.textLineHeight : 1
        wrapMode: Text.WordWrap
    }

    background: Rectangle {
        radius: IKMainWindow.syncSelectorTooltipRadius
        color: root.surfaceColor
    }
}
