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

// Search dialog of the main window, copied from the macOS search sheet. It follows the IKModal conventions (overlay
// parent, inset scrim, transitions) without its title and footer.
Popup {
    id: root

    required property var controller
    // The overlay spans the complete native window: the scrim must not dim its transparent shadow margin.
    property real scrimInset: 0
    property real scrimRadius: 0

    readonly property int stateLoading: SearchController.Loading
    readonly property int stateResults: SearchController.Results
    readonly property int stateEmpty: SearchController.Empty
    readonly property int stateError: SearchController.Error
    readonly property bool showsList: resultList.count > 0
                                      && (root.controller.state === root.stateResults
                                          || root.controller.state === root.stateLoading)

    parent: Overlay.overlay
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    width: parent ? Math.min(IKSearch.dialogWidth, Math.max(0, parent.width - 2 * IKModalTokens.screenMargin))
                  : IKSearch.dialogWidth
    height: parent ? Math.min(IKSearch.dialogHeight, Math.max(0, parent.height - 2 * IKModalTokens.screenMargin))
                   : IKSearch.dialogHeight
    padding: 0
    popupType: Popup.Item
    modal: true
    dim: true
    focus: true
    closePolicy: Popup.CloseOnPressOutside

    onAboutToShow: root.controller.open()
    onOpened: Qt.callLater(searchField.focusInput)
    onClosed: root.controller.close()

    Shortcut {
        sequences: [ StandardKey.Cancel ]
        enabled: root.opened
        context: Qt.WindowShortcut
        onActivated: root.close()
    }

    Overlay.modal: Item {
        // The popup only writes the scrim opacity (1 when opening, 0 when closing): this behavior fades it with the
        // exact duration and easing of the matching popup transition, so the scrim and the card move together.
        Behavior on opacity {
            id: scrimFade

            NumberAnimation {
                duration: scrimFade.targetValue > 0 ? IKModalTokens.enterDuration : IKModalTokens.exitDuration
                easing.type: scrimFade.targetValue > 0 ? Easing.OutCubic : Easing.InCubic
            }
        }

        Rectangle {
            x: root.scrimInset
            y: root.scrimInset
            width: Math.max(0, parent.width - 2 * root.scrimInset)
            height: Math.max(0, parent.height - 2 * root.scrimInset)
            radius: root.scrimRadius
            color: IKColors.modalScrim
        }
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: IKModalTokens.enterDuration
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "scale"
                from: IKModalTokens.enterScale
                to: 1
                duration: IKModalTokens.enterDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1
            to: 0
            duration: IKModalTokens.exitDuration
            easing.type: Easing.InCubic
        }
    }

    background: Rectangle {
        radius: IKRadius.r16
        color: IKColors.modalSurface
        border.width: IKModalTokens.borderWidth
        border.color: IKColors.modalBorder
    }

    contentItem: Item {
        Accessible.role: Accessible.Dialog
        Accessible.name: qsTrId("accessibilitySearchSheetLabel")

        SearchField {
            id: searchField

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: IKSearch.fieldMargin
            controller: root.controller
            onMoveToResultsRequested: {
                if (root.showsList) {
                    resultList.currentIndex = 0;
                    resultList.forceActiveFocus();
                }
            }
        }

        Item {
            id: body

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: searchField.bottom
            anchors.bottom: parent.bottom
            clip: true

            ListView {
                id: resultList

                // Requests the next page once few loaded rows remain below the viewport, so that it usually arrives
                // before the user reaches the end of the list.
                function loadMoreIfNeeded() {
                    if (!root.controller.canLoadMore || root.controller.loadingMore) {
                        return;
                    }

                    // No row under the bottom edge means the viewport already shows the end of the list (or the footer).
                    const bottomIndex = indexAt(width / 2, contentY + height - 1);
                    const lastVisibleIndex = bottomIndex < 0 ? count - 1 : bottomIndex;
                    if (count - 1 - lastVisibleIndex < IKSearch.loadMoreRemainingRows) {
                        root.controller.loadMore();
                    }
                }

                anchors.fill: parent
                anchors.leftMargin: IKSearch.listHorizontalMargin
                anchors.rightMargin: IKSearch.listHorizontalMargin
                visible: root.showsList
                model: root.controller.model
                boundsBehavior: Flickable.StopAtBounds
                keyNavigationEnabled: true
                currentIndex: -1
                // Drawn in the list's right margin rather than over the rows, where it would cover the trailing web icon.
                ScrollBar.vertical: ScrollBar {
                    parent: scrollBarGutter
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    policy: ScrollBar.AsNeeded
                }

                delegate: SearchResultRow {
                    width: ListView.view.width
                    current: ListView.isCurrentItem && ListView.view.activeFocus
                    onActivated: revealInFolder => root.controller.openResult(index, revealInFolder)
                }

                footer: Item {
                    width: ListView.view ? ListView.view.width : 0
                    height: root.controller.loadingMore || root.controller.nextPageFailed
                            ? IKSearch.footerHeight : 0

                    IKLoadingSpinner {
                        anchors.centerIn: parent
                        width: IKIconSizes.medium
                        height: IKIconSizes.medium
                        visible: root.controller.loadingMore
                    }

                    Row {
                        anchors.centerIn: parent
                        spacing: IKSpacing.s8
                        visible: root.controller.nextPageFailed && !root.controller.loadingMore

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTrId("unexpectedErrorTeachingTipTitle")
                            color: IKColors.textSecondary
                            font.pixelSize: IKFonts.bodySize
                        }

                        IKLinkButton {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTrId("buttonRetry")
                            onClicked: root.controller.retry()
                        }
                    }
                }

                Keys.onReturnPressed: event => root.controller.openResult(currentIndex,
                                                                          (event.modifiers & Qt.ControlModifier) !== 0)
                Keys.onEnterPressed: event => root.controller.openResult(currentIndex,
                                                                         (event.modifiers & Qt.ControlModifier) !== 0)
                Keys.onUpPressed: event => {
                    if (currentIndex <= 0) {
                        searchField.focusInput();
                        return;
                    }
                    event.accepted = false;
                }
                Keys.onDownPressed: event => {
                    if (currentIndex >= count - 1) {
                        // Keeps the footer Retry reachable from the keyboard.
                        if (root.controller.nextPageFailed) {
                            root.controller.retry();
                        } else {
                            root.controller.loadMore();
                        }
                        return;
                    }
                    event.accepted = false;
                }

                onContentYChanged: loadMoreIfNeeded()
                onHeightChanged: loadMoreIfNeeded()
                // A short page may not fill the viewport, and then no scrolling would ever request the next one.
                onCountChanged: Qt.callLater(loadMoreIfNeeded)

                Connections {
                    target: root.controller

                    function onPaginationChanged() {
                        Qt.callLater(resultList.loadMoreIfNeeded);
                    }

                    function onStateChanged() {
                        if (root.controller.state !== root.stateResults) {
                            resultList.currentIndex = -1;
                        }
                    }
                }
            }

            // Right margin of the list, which hosts its scroll bar; hidden with the list in the empty and error states.
            Item {
                id: scrollBarGutter

                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: IKSearch.listHorizontalMargin
                visible: resultList.visible
            }

            SearchPlaceholderRows {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: IKSearch.listHorizontalMargin
                anchors.rightMargin: IKSearch.listHorizontalMargin
                visible: root.controller.state === root.stateLoading && !root.showsList
            }

            SearchStateView {
                anchors.fill: parent
                visible: !root.showsList && root.controller.state !== root.stateLoading
                title: {
                    switch (root.controller.state) {
                    case root.stateEmpty:
                        return qsTrId("noResultsFound");
                    case root.stateError:
                        return qsTrId("unexpectedErrorTeachingTipTitle");
                    default:
                        return qsTrId("searchYourFiles");
                    }
                }
                subtitle: {
                    switch (root.controller.state) {
                    case root.stateEmpty:
                        return qsTrId("tryDifferentSearchTerm");
                    case root.stateError:
                        return qsTrId("unexpectedErrorTeachingTipContent");
                    default:
                        return qsTrId("typeToStartSearching");
                    }
                }
                actionText: root.controller.state === root.stateError ? qsTrId("buttonRetry") : ""
                onActionTriggered: root.controller.retry()
            }
        }
    }
}
