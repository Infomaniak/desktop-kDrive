/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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
    enum Value {
        System,
        Light,
        Dark
    }

    property int _mode: ThemeMode.System

    function set(mode) {
        if (mode === ThemeMode.System || mode === ThemeMode.Light || mode === ThemeMode.Dark)
            _mode = mode // TODO add logging if the mode arg is invalid.
    }

    readonly property bool isDark: {
        if (_mode === ThemeMode.Light) return false
        if (_mode === ThemeMode.Dark)  return true
        return Qt.styleHints.colorScheme === Qt.Dark
    }
}