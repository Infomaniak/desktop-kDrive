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

import QtQuick
import QtQuick.VectorImage
import kDrive.UI

Item {
    Column {
        anchors.centerIn: parent
        spacing: IKWaiting.contentSpacing

        VectorImage {
            width: IKWaiting.logoSize
            height: IKWaiting.logoSize
            source: "qrc:/assets/taskbar/logo_kdrive.svg"
            fillMode: VectorImage.PreserveAspectFit
            preferredRendererType: VectorImage.CurveRenderer
        }

        IKLoadingSpinner {
            anchors.horizontalCenter: parent.horizontalCenter
            width: IKWaiting.spinnerSize
            height: IKWaiting.spinnerSize
            strokeWidth: IKWaiting.spinnerStrokeWidth
            cycleDuration: IKWaiting.spinnerCycleDuration
        }
    }
}
