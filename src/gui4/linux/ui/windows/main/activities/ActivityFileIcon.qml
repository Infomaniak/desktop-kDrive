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
import kDrive.UI

Item {
    id: root

    required property string fileIconName
    readonly property url fileSource: root.isDirectory ? "" : "qrc:/assets/main/activities/" + root.fileIconName + (ThemeMode.isDark ? "-dark" : "") + ".svg"
    required property bool isDirectory

    height: IKActivities.fileIconSize
    width: IKActivities.fileIconSize

    IKTintedIcon {
        anchors.fill: parent
        color: IKColors.activitiesFileIcon
        source: "qrc:/assets/main/folder.svg"
        visible: root.isDirectory
    }
    Image {
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        source: root.fileSource
        sourceSize.height: root.height
        sourceSize.width: root.width
        visible: !root.isDirectory
    }
}
