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

    Column {
        anchors.centerIn: parent
        spacing: IKActivities.emptyContentSpacing

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            width: IKActivities.emptyIllustrationWidth
            height: IKActivities.emptyIllustrationHeight
            source: ThemeMode.isDark ? "qrc:/assets/main/activities/empty-dark.svg" : "qrc:/assets/main/activities/empty.svg"
            fillMode: Image.PreserveAspectFit
            sourceSize.width: width
            sourceSize.height: height
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: IKActivities.emptyTextSpacing

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTrId("unavailableContentNoActivityTitle")
                color: IKColors.textPrimary
                font.pixelSize: IKActivities.emptyTitleSize
                font.weight: IKFonts.emphasized
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTrId("unavailableContentNoActivityDescription")
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.bodySize
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
