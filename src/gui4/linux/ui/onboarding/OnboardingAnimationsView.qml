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
import kDrive.UI

Item {
    id: root

    readonly property url lottieSource: ThemeMode.isDark
        ? "qrc:/assets/onboarding/lotties/dark/loader-stroke.lottie"
        : "qrc:/assets/onboarding/lotties/light/loader-stroke.lottie"

    // Qt's Lottie renderer is not linked in the Linux target yet. Keep the onboarding panel wired to the asset
    // source so the renderer can be dropped in here without changing the screen structure.
    Item {
        id: lottieSlot

        anchors.fill: parent
    }
}
