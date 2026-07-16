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

pragma Singleton
import QtQuick

QtObject {

    readonly property bool darkMode: ThemeMode.isDark

    // -------------------------------------------------------------------------
    // T1 — Primitives
    // -------------------------------------------------------------------------

    component Primitives: QtObject {
        readonly property color white: "#FFFFFF"

        // Blue
        readonly property color blue100: "#E2EDFD"
        readonly property color blue400: "#8AA7EF"
        readonly property color blue500: "#6E88E6"
        readonly property color blue600: "#446FDD"
        readonly property color blue950: "#1F2547"

        // NeutralBlue
        readonly property color neutralBlue50: "#F6F8FC"
        readonly property color neutralBlue100: "#EDF1F8"
        readonly property color neutralBlue200: "#DCE3F0"
        readonly property color neutralBlue300: "#C5CDDB"
        readonly property color neutralBlue500: "#424752"
        readonly property color neutralBlue600: "#1F242E"
        readonly property color neutralBlue700: "#191D24"
        readonly property color neutralBlue800: "#101419"
        readonly property color neutralBlue900: "#0C0F13"

        // Gray
        readonly property color gray50: "#F7F7F7"
        readonly property color gray100: "#F5F5F5"
        readonly property color gray400: "#6A768B"
        readonly property color gray500: "#484E5B"
        readonly property color gray600: "#9F9F9F"
        readonly property color gray950: "#0A0C0F"

        // Red
        readonly property color red500: "#E33D4E"
        readonly property color red600: "#C92F40"

        // Green
        readonly property color green400: "#5FC96B"
        readonly property color green500: "#3EBF4D"

        // Orange
        readonly property color orange100: "#FFF5D3"
        readonly property color orange300: "#FFD46D"
        readonly property color orange400: "#FFB632"
        readonly property color orange500: "#FF9D0A"
        readonly property color orange800: "#A14B0B"

        // Brown
        readonly property color brown950: "#461E04"

        // Brand
        readonly property color infomaniak: "#0098FF"
        readonly property color kDrive500: "#5C89F7"
        readonly property color kDrive600: "#446FDD"
    }

    readonly property Primitives _p: Primitives {}

    // -------------------------------------------------------------------------
    // T2 — Semantic tokens
    // -------------------------------------------------------------------------

    // Accent
    readonly property color accentPrimary: darkMode ? _p.blue400 : _p.blue600
    readonly property color accentSecondary: darkMode ? _p.kDrive600 : _p.kDrive500

    // Action
    readonly property color actionPrimary: accentPrimary
    readonly property color actionOnPrimary: darkMode ? _p.neutralBlue800 : _p.neutralBlue100
    readonly property color actionDisabled: _p.gray400

    // Text
    readonly property color textPrimary: darkMode ? _p.neutralBlue50 : _p.neutralBlue800
    readonly property color textSecondary: darkMode ? _p.neutralBlue200 : _p.neutralBlue600
    readonly property color textTertiary: darkMode ? _p.neutralBlue300 : _p.neutralBlue500

    // Surface
    readonly property color surfacePrimary: darkMode ? _p.neutralBlue800 : _p.neutralBlue50
    readonly property color surfaceSecondary: darkMode ? _p.neutralBlue700 : _p.neutralBlue100
    readonly property color surfaceTertiary: darkMode ? _p.neutralBlue600 : _p.neutralBlue200
    readonly property color onboardingSurfacePrimary: darkMode ? _p.neutralBlue900 : _p.white
    readonly property color onboardingSurfaceSecondary: darkMode ? _p.neutralBlue700 : _p.gray50
    readonly property color onboardingUserBadgeSurface: darkMode ? _p.neutralBlue700 : _p.gray100
    readonly property color onboardingDriveCellSurface: darkMode ? _p.neutralBlue700 : _p.neutralBlue50
    readonly property color onboardingDriveDisabledText: _p.gray600
    readonly property color tooltipSurface: darkMode ? _p.neutralBlue50 : _p.neutralBlue800
    readonly property color tooltipText: darkMode ? _p.neutralBlue800 : _p.white
    readonly property color onboardingTooltipSurface: tooltipSurface
    readonly property color onboardingTooltipText: tooltipText
    readonly property color onboardingDriveIconBorder: Qt.rgba(0.10, 0.10, 0.10, 0.35)
    readonly property color onboardingDriveIconHighlightStart: Qt.rgba(1, 1, 1, 0.20)
    readonly property color onboardingDriveIconHighlightEnd: Qt.rgba(1, 1, 1, 0)
    readonly property color onboardingDriveIconInnerBorder: Qt.rgba(1, 1, 1, 0.25)

    // Window chrome
    readonly property color windowControlHover: surfaceSecondary
    readonly property color windowControlPressed: surfaceTertiary
    readonly property color windowControlCloseHover: _p.red500
    readonly property color windowControlClosePressed: _p.red600
    readonly property color windowControlIcon: textPrimary
    readonly property color windowControlIconOnClose: _p.white

    // Status — Strong
    readonly property color statusStrongWarning: darkMode ? _p.orange300 : _p.orange800

    // Status — Medium
    readonly property color statusMediumSuccess: darkMode ? _p.green400 : _p.green500
    readonly property color statusMediumWarning: darkMode ? _p.orange400 : _p.orange500
    readonly property color statusMediumSecurity: darkMode ? _p.blue400 : _p.blue500
    readonly property color statusMediumNeutral: darkMode ? _p.gray400 : _p.gray500

    // Status — Light
    readonly property color statusLightWarning: darkMode ? _p.brown950 : _p.orange100
    readonly property color statusLightSecurity: darkMode ? _p.blue950 : _p.blue100
    readonly property color statusLightNeutral: darkMode ? _p.gray950 : _p.neutralBlue100

    // -------------------------------------------------------------------------
    // T3 — Contextual tokens
    // -------------------------------------------------------------------------

    readonly property color driveDefaultColor: _p.infomaniak
}
