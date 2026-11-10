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
import kDrive.UI

// Hosts the Activities sub-pages and follows ActivitiesNavigation.currentPage.
Item {
    id: root

    required property var controller
    required property var errorsPageController
    required property var navigation
    property real modalScrimInset: 0
    property real modalScrimRadius: 0

    // One component per page of the ActivitiesNavigation chain, in chain order: the stack depth of a page is its index + 1.
    readonly property list<Component> pageComponents: [listPageComponent, errorsPageComponent, conflictsQuickPageComponent,
        conflictsListPageComponent]

    // `moveFocus`: a page change requested by the user gives the keyboard focus to the new page, so that it does not stay
    // on a control that disappeared, such as the toolbar back button. Rebuilding the view leaves the focus where it is.
    function syncStack(operation, moveFocus) {
        if (stack.busy) {
            return;
        }

        const targetDepth = Math.min(root.navigation.currentPage + 1, root.pageComponents.length);
        const changed = stack.depth !== targetDepth;
        if (stack.depth > targetDepth) {
            stack.pop(stack.get(targetDepth - 1), operation);
        }
        while (stack.depth < targetDepth) {
            stack.push(root.pageComponents[stack.depth], {}, operation);
        }
        if (moveFocus && changed && stack.currentItem) {
            stack.currentItem.focusDefault();
        }
    }

    clip: true

    StackView {
        id: stack

        anchors.fill: parent

        // A page change during a transition is applied once it ends.
        onBusyChanged: {
            if (!busy) {
                root.syncStack(StackView.Transition, true);
            }
        }
    }

    Connections {
        target: root.navigation

        function onCurrentPageChanged() {
            root.syncStack(StackView.Transition, true);
        }
    }

    // Builds the whole stack, list included, without animation: the view is recreated when the tab or the window comes back.
    Component.onCompleted: root.syncStack(StackView.Immediate, false)

    Component {
        id: listPageComponent

        ActivitiesListPage {
            controller: root.controller
        }
    }

    Component {
        id: errorsPageComponent

        ErrorsPage {
            controller: root.errorsPageController
            conflictVersionDialog: sharedConflictVersionDialog
            modalScrimInset: root.modalScrimInset
            modalScrimRadius: root.modalScrimRadius
        }
    }

    Component {
        id: conflictsQuickPageComponent

        ConflictsQuickPage {
            controller: root.errorsPageController.conflicts
        }
    }

    Component {
        id: conflictsListPageComponent

        ConflictsListPage {
            controller: root.errorsPageController.conflicts
            conflictVersionDialog: sharedConflictVersionDialog
        }
    }

    // One dialog for the errors page and the conflicts list, which live in the stack at the same time: two dialogs on the
    // same controller would open together.
    ConflictVersionDialog {
        id: sharedConflictVersionDialog

        controller: root.errorsPageController.conflictVersion
        scrimInset: root.modalScrimInset
        scrimRadius: root.modalScrimRadius
        onFallbackFocusRequested: {
            if (stack.currentItem) {
                stack.currentItem.focusDefault();
            }
        }
    }
}
