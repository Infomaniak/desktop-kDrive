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

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Item {
    id: root

    clip: true

    required property Component initialItem
    readonly property int depth: stack.depth
    readonly property var currentItem: stack.currentItem
    readonly property bool canGoBack: depth > 1 && !stack.busy
    readonly property string currentTitle: stack.currentItem ? stack.currentItem["navigationTitle"] || "" : ""
    readonly property string previousTitle: {
        const previousItem = depth > 1 ? stack.get(depth - 2) : null;
        return previousItem ? previousItem["navigationTitle"] || "" : "";
    }

    property var _focusHistory: []
    // Reset asked during a transition, applied once it ends: {component, pushedComponent}.
    property var _pendingReset: null

    signal focusRestorationRequested(var target)

    function push(component, properties) {
        if (stack.busy) {
            return;
        }

        const activeFocusItem = root.Window.window ? root.Window.window.activeFocusItem : null;
        root._focusHistory.push(activeFocusItem);
        stack.push(component, properties || {});
    }

    function pop() {
        if (!canGoBack) {
            return;
        }

        const focusTarget = root._focusHistory.pop();
        stack.pop();
        Qt.callLater(() => root.focusRestorationRequested(focusTarget));
    }

    // Shows `component` alone, then pushes `pushedComponent` over it when given.
    function reset(component, pushedComponent) {
        // A reset can come from an asynchronous event: defer it, with its page, until the transition ends rather than
        // dropping it.
        if (stack.busy) {
            root._pendingReset = {
                "component": component,
                "pushedComponent": pushedComponent || null
            };
            return;
        }

        root._pendingReset = null;
        root._focusHistory = [];
        stack.clear(StackView.Immediate);
        stack.push(component, {}, StackView.Immediate);
        if (pushedComponent) {
            root.push(pushedComponent);
        }
    }

    StackView {
        id: stack

        anchors.fill: parent
        clip: true
        initialItem: root.initialItem

        onBusyChanged: {
            if (!busy && root._pendingReset) {
                root.reset(root._pendingReset.component, root._pendingReset.pushedComponent);
            }
        }
    }
}
