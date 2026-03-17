pragma Singleton
import QtQuick

QtObject {
    // T1 — Primitives
    readonly property real s2:  2
    readonly property real s4:  4
    readonly property real s8:  8
    readonly property real s12: 12
    readonly property real s16: 16
    readonly property real s24: 24
    readonly property real s32: 32
    readonly property real s48: 48
    readonly property real s64: 64

    // T2 — Semantic tokens
    readonly property real page: s24
}