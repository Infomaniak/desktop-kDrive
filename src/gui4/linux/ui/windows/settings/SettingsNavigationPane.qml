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
    // Stays true during a transition, so a back button does not blink between two pages that both have one.
    readonly property bool hasPreviousPage: depth > 1
    readonly property bool canGoBack: hasPreviousPage && !stack.busy
    readonly property string currentTitle: stack.currentItem ? stack.currentItem["navigationTitle"] || "" : ""
    readonly property string currentScreenId: stack.currentItem ? stack.currentItem["screenId"] || "" : ""
    readonly property string previousTitle: {
        const previousItem = depth > 1 ? stack.get(depth - 2) : null;
        return previousItem ? previousItem["navigationTitle"] || "" : "";
    }

    property var _focusHistory: []
    // Reset asked during a transition, applied once it ends: {component, pushedComponent}.
    property var _pendingReset: null
    property var _pendingPagesAboveRoot: null
    readonly property bool navigationPending: _pendingReset !== null || _pendingPagesAboveRoot !== null

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

    // Shows the given pages above the root page, `{component, properties}` from the bottom up, and keeps the root page
    // with its state. An animated navigation transitions from the current page, like a push or a pop; otherwise the
    // pages appear at once. The pages below the new current page have no focus to restore when going back. `onApplied`,
    // optional, runs once the navigation has started, which a transition in progress can defer.
    function showAboveRoot(pages, animated, onApplied) {
        // The request can come from outside the window: defer it until the transition ends rather than dropping it.
        if (stack.busy) {
            root._pendingReset = null;
            root._pendingPagesAboveRoot = {"pages": pages, "animated": animated, "onApplied": onApplied};
            return;
        }

        root._pendingReset = null;
        root._pendingPagesAboveRoot = null;
        root._focusHistory = pages.map(() => null);
        if (pages.length === 0) {
            if (stack.depth > 1) {
                stack.popToIndex(0, animated ? StackView.PopTransition : StackView.Immediate);
            }
            if (onApplied) {
                onApplied();
            }
            return;
        }

        const items = [];
        for (const page of pages) {
            items.push(page.component, page.properties || {});
        }
        if (stack.depth > 1) {
            stack.replace(stack.get(1, StackView.ForceLoad), items,
                          animated ? StackView.ReplaceTransition : StackView.Immediate);
        } else {
            stack.pushItems(items, animated ? StackView.PushTransition : StackView.Immediate);
        }

        // StackView only creates the top page, while a page below can still have a role: the drive management page
        // leaves a drive that lost its last synchronization. A page loaded this way starts visible, above which the top
        // page would be drawn; StackView shows it again when going back.
        for (let index = 1; index < stack.depth - 1; ++index) {
            stack.get(index, StackView.ForceLoad).visible = false;
        }
        if (onApplied) {
            onApplied();
        }
    }

    function rootItem() {
        return stack.get(0, StackView.ForceLoad);
    }

    // Shows `component` alone, then pushes `pushedComponent` over it when given.
    function reset(component, pushedComponent) {
        // A reset can come from an asynchronous event: defer it, with its page, until the transition ends rather than
        // dropping it.
        if (stack.busy) {
            root._pendingPagesAboveRoot = null;
            root._pendingReset = {
                "component": component,
                "pushedComponent": pushedComponent || null
            };
            return;
        }

        root._pendingPagesAboveRoot = null;
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
            if (busy) {
                return;
            }

            if (root._pendingReset) {
                root.reset(root._pendingReset.component, root._pendingReset.pushedComponent);
            } else if (root._pendingPagesAboveRoot) {
                const pending = root._pendingPagesAboveRoot;
                root.showAboveRoot(pending.pages, pending.animated, pending.onApplied);
            }
        }
    }
}
