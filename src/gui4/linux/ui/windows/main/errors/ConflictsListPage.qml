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
import QtQuick.Controls.Basic
import QtQuick.Layouts
import kDrive.UI

// "Version conflicts" list page: every conflict of the selected synchronization, searchable, each opening the version
// selection dialog on its file.
FocusScope {
    id: root

    required property var controller
    // Shared with the errors page: restores the focus on the control that opened it.
    required property var conflictVersionDialog

    readonly property var listModel: root.controller.list

    function focusDefault() {
        searchField.focusInput();
    }

    // Enter or Space on the list: the version of its current row.
    function chooseCurrentRow() {
        const row = list.currentItem;
        if (row && !row.actionPending) {
            root.openVersionDialog(() => root.controller.chooseVersion(row.rowId));
        }
    }

    function openVersionDialog(openFrom) {
        root.conflictVersionDialog.returnFocusItem = root.Window.activeFocusItem;
        openFrom();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: IKActivities.pagePadding
        spacing: IKActivities.conflictsSectionSpacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: IKActivities.conflictsHeaderSpacing

            Text {
                Layout.fillWidth: true
                text: qsTrId("labelManyConflictTitle").arg(root.controller.count.toLocaleString(Qt.locale(), "f", 0))
                textFormat: Text.PlainText
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                wrapMode: Text.WordWrap
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Text {
                Layout.fillWidth: true
                text: qsTrId("labelChooseConflictVersionIndividual")
                textFormat: Text.PlainText
                color: IKColors.textTertiary
                font.pixelSize: IKFonts.subheadlineSize
                wrapMode: Text.WordWrap
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: IKActivities.conflictsListSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: IKActivities.conflictsButtonSpacing

                IKSearchField {
                    id: searchField

                    Layout.preferredWidth: IKActivities.conflictsSearchWidth
                    Layout.preferredHeight: IKActivities.conflictsSearchHeight
                    radius: height / 2
                    placeholderText: qsTrId("conflictsSearchBoxPlaceholder")
                    accessibleName: qsTrId("conflictsSearchBoxPlaceholder")
                    Component.onCompleted: text = root.listModel.searchText
                    onTextEdited: editedText => root.listModel.searchText = editedText
                    onCleared: root.listModel.searchText = ""
                }

                Item {
                    Layout.fillWidth: true
                }

                IKModalButton {
                    implicitHeight: IKActivities.conflictsButtonHeight
                    text: qsTrId("buttonStartConflictsResolution")
                    actionEnabled: root.controller.count > 0 && !root.controller.resolutionPending
                    onClicked: root.openVersionDialog(() => root.controller.startChoices())
                }
            }

            // As tall as its rows, up to the space left.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Rectangle {
                    width: parent.width
                    height: Math.min(parent.height,
                                     Math.max(IKActivities.conflictsRowHeight, list.contentHeight) + 2 * IKActivities.conflictsListPadding)
                    radius: IKActivities.conflictsListRadius
                    color: IKColors.conflictsListSurface

                    ListView {
                        id: list

                        anchors.fill: parent
                        anchors.margins: IKActivities.conflictsListPadding
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        // A single tab stop: Up and Down move the current row, which the view keeps in sight, so every
                        // row is reachable even when its delegate is not instantiated yet.
                        activeFocusOnTab: true
                        keyNavigationEnabled: true
                        currentIndex: -1
                        model: root.listModel
                        Accessible.role: Accessible.List
                        Accessible.name: qsTrId("labelChooseConflictVersionIndividual")
                        onActiveFocusChanged: {
                            if (activeFocus && currentIndex < 0 && count > 0) {
                                currentIndex = 0;
                            }
                        }
                        onCurrentIndexChanged: {
                            if (currentIndex >= 0) {
                                positionViewAtIndex(currentIndex, ListView.Contain);
                            }
                        }
                        Keys.onReturnPressed: root.chooseCurrentRow()
                        Keys.onEnterPressed: root.chooseCurrentRow()
                        Keys.onSpacePressed: root.chooseCurrentRow()
                        delegate: ConflictListRow {
                            width: ListView.view.width
                            isLast: index === root.listModel.count - 1
                            keyboardCurrent: ListView.isCurrentItem && list.activeFocus
                            onChooseRequested: rowId => root.openVersionDialog(() => root.controller.chooseVersion(rowId))
                        }

                        ScrollBar.vertical: ScrollBar {}
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: root.listModel.count === 0
                        text: qsTrId("noResultsFound")
                        textFormat: Text.PlainText
                        color: IKColors.textSecondary
                        font.pixelSize: IKFonts.bodySize
                    }
                }
            }
        }
    }
}
