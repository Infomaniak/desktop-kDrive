pragma Singleton
import QtQuick

QtObject {
    // Sizes (in pt)
    readonly property real largeTitleSize: 26
    readonly property real titleSize:      22
    readonly property real title2Size:     20
    readonly property real title3Size:     17
    readonly property real headlineSize:   14
    readonly property real bodySize:       13
    readonly property real subheadlineSize: 11

    // Weights
    readonly property int regular:    Font.Normal
    readonly property int emphasized: Font.DemiBold
}