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
            _mode = mode
    }

    readonly property bool isDark: {
        if (_mode === ThemeMode.Light) return false
        if (_mode === ThemeMode.Dark)  return true
        return Qt.styleHints.colorScheme === Qt.Dark
    }
}