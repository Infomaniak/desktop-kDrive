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
import QtQml.Models
import kDrive.UI

Rectangle {
    id: root

    required property var treeModel

    // The tree itself takes the keyboard focus; the surrounding frame is decoration.
    readonly property Item keyboardFocusItem: treeView
    readonly property bool contentVisible: !treeModel.loading && !treeModel.loadFailed && !treeModel.empty

    implicitHeight: IKSyncConfiguration.treeHeight
    radius: IKSyncConfiguration.treeRadius
    color: IKColors.syncConfigurationTreeSurface
    border.width: IKSyncConfiguration.treeBorderWidth
    border.color: IKColors.syncConfigurationDivider

    Rectangle {
        id: header

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: IKSyncConfiguration.treeHeaderHeight
        radius: root.radius
        color: IKColors.syncConfigurationTreeHeaderSurface

        // Squares off the bottom corners that the shared radius would otherwise round inside the container.
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.radius
            color: parent.color
        }

        IKCheckBox {
            id: rootCheckBox

            anchors.left: parent.left
            anchors.leftMargin: IKSyncConfiguration.treeContentPadding
            anchors.verticalCenter: parent.verticalCenter
            checkState: root.treeModel.rootCheckState
            enabled: root.contentVisible
            Accessible.name: qsTrId("labelAllFolders")
            onClicked: root.treeModel.toggleRootSelection()
        }

        Text {
            // Aligned with the folder icon of a root-level row rather than with its checkbox.
            anchors.left: rootCheckBox.right
            anchors.leftMargin: IKSyncConfiguration.treeDisclosureSize + IKSpacing.s4
            anchors.right: headerSizeLabel.left
            anchors.rightMargin: IKSyncConfiguration.treeRowSpacing
            anchors.verticalCenter: parent.verticalCenter
            text: qsTrId("labelName")
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.subheadlineSize
            font.weight: IKFonts.emphasized
            elide: Text.ElideRight
        }

        Text {
            id: headerSizeLabel

            width: IKSyncConfiguration.treeSizeColumnWidth
            anchors.right: parent.right
            anchors.rightMargin: IKSyncConfiguration.treeStateSpacing
            anchors.verticalCenter: parent.verticalCenter
            text: qsTrId("labelSize")
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.subheadlineSize
            font.weight: IKFonts.emphasized
            horizontalAlignment: Text.AlignRight
        }
    }

    Rectangle {
        id: headerSeparator

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        height: IKSyncConfiguration.treeBorderWidth
        color: IKColors.syncConfigurationDivider
    }

    Item {
        id: body

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: headerSeparator.bottom
        anchors.bottom: parent.bottom

        IKLoadingSpinner {
            anchors.centerIn: parent
            visible: root.treeModel.loading
            width: IKIconSizes.large
            height: width
            color: IKColors.actionPrimary
        }

        Column {
            anchors.centerIn: parent
            width: parent.width - 2 * IKSpacing.s24
            spacing: IKSyncConfiguration.treeStateSpacing
            visible: root.treeModel.loadFailed

            Text {
                width: parent.width
                text: qsTrId("onboardingLoginErrorDescription")
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.bodySize
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            IKModalButton {
                anchors.horizontalCenter: parent.horizontalCenter
                role: IKModalButton.Secondary
                text: qsTrId("buttonRetry")
                onClicked: root.treeModel.retryRoot()
            }
        }

        Column {
            anchors.centerIn: parent
            width: parent.width - 2 * IKSpacing.s24
            spacing: IKSyncConfiguration.treeStateSpacing
            visible: root.treeModel.empty

            Text {
                width: parent.width
                text: qsTrId("labelSyncExclusionEmptyTitle")
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.emphasized
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: qsTrId("labelSyncExclusionEmptyDescription")
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.subheadlineSize
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }

        TreeView {
            id: treeView

            // The tree is a single tab stop that moves a current row internally, like a native tree: tabbing through
            // every checkbox of a large folder list would make the page unusable from the keyboard. TableView already
            // tracks that row as `currentRow`, driven by the selection model below.
            function moveCurrentToRow(targetRow: int): void {
                if (targetRow < 0 || targetRow >= treeView.rows) {
                    return
                }
                treeSelection.setCurrentIndex(treeView.index(targetRow, 0), ItemSelectionModel.NoUpdate)
                treeView.positionViewAtRow(targetRow, TableView.Contain)
            }

            anchors.fill: parent
            anchors.margins: IKSyncConfiguration.treeContentPadding
            visible: root.contentVisible
            clip: true
            model: root.treeModel
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: true
            // Arrow keys are handled below so collapsing and expanding behave the same on every Qt version.
            keyNavigationEnabled: false
            selectionModel: ItemSelectionModel {
                id: treeSelection

                model: root.treeModel
            }

            onActiveFocusChanged: {
                if (treeView.activeFocus && treeView.currentRow < 0) {
                    treeView.moveCurrentToRow(0)
                }
            }

            Keys.onUpPressed: treeView.moveCurrentToRow(treeView.currentRow - 1)
            Keys.onDownPressed: treeView.moveCurrentToRow(treeView.currentRow + 1)
            Keys.onLeftPressed: {
                const row = treeView.currentRow
                if (row < 0) {
                    return
                }
                if (treeView.isExpanded(row)) {
                    treeView.collapse(row)
                    return
                }
                treeView.moveCurrentToRow(treeView.rowAtIndex(root.treeModel.parentIndex(treeSelection.currentIndex)))
            }
            Keys.onRightPressed: {
                const row = treeView.currentRow
                if (row < 0) {
                    return
                }
                if (!treeView.isExpanded(row)) {
                    treeView.expand(row)
                    return
                }
                treeView.moveCurrentToRow(row + 1)
            }
            Keys.onSpacePressed: root.treeModel.toggleSelection(treeSelection.currentIndex)
            Keys.onReturnPressed: root.treeModel.toggleSelection(treeSelection.currentIndex)
            Keys.onEnterPressed: root.treeModel.toggleSelection(treeSelection.currentIndex)

            ScrollBar.vertical: ScrollBar {
                policy: treeView.contentHeight > treeView.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            }

            delegate: Item {
                id: folderRow

                required property TreeView treeView
                required property bool isTreeNode
                required property bool expanded
                required property bool hasChildren
                required property int depth
                required property int row
                required property int column
                required property string folderName
                required property int checkState
                required property bool accessDenied
                required property string sizeText
                required property bool childrenLoading
                required property bool childrenLoadFailed
                required property bool current

                readonly property var treeIndex: folderRow.treeView.index(folderRow.row, folderRow.column)
                readonly property bool disclosureVisible: folderRow.isTreeNode && folderRow.hasChildren

                implicitWidth: folderRow.treeView.width
                implicitHeight: IKSyncConfiguration.treeRowHeight

                // The row carries the accessible state; its checkbox and disclosure are decorations of that state
                // rather than separate controls, so a screen reader announces one folder instead of three items.
                Accessible.role: Accessible.TreeItem
                Accessible.name: folderRow.folderName
                Accessible.description: folderRow.sizeText
                Accessible.checkable: !folderRow.accessDenied
                Accessible.checked: folderRow.checkState === Qt.Checked
                Accessible.checkStateMixed: folderRow.checkState === Qt.PartiallyChecked
                Accessible.readOnly: folderRow.accessDenied

                Component.onCompleted: root.treeModel.setRowVisible(folderRow.treeIndex, true)
                Component.onDestruction: root.treeModel.setRowVisible(folderRow.treeIndex, false)
                TableView.onPooled: root.treeModel.setRowVisible(folderRow.treeIndex, false)
                TableView.onReused: root.treeModel.setRowVisible(folderRow.treeIndex, true)

                Rectangle {
                    anchors.fill: parent
                    radius: IKSyncConfiguration.treeRowRadius
    // The current row is a navigation cursor, not a focus ring: TreeView is a Flickable and has no
                    // `visualFocus`, so a border here would also show on a plain mouse click. A tint keeps the two
                    // meanings apart while staying useful to mouse users.
                    color: {
                        if (folderRow.current && folderRow.treeView.activeFocus) {
                            return IKColors.syncConfigurationRowCurrent
                        }
                        if (rowHover.hovered) {
                            return IKColors.syncConfigurationRowHover
                        }
                        return folderRow.checkState === Qt.Unchecked ? "transparent"
                                                                     : IKColors.syncConfigurationRowSelectedSurface
                    }
                }

                HoverHandler {
                    id: rowHover
                }

                IKCheckBox {
                    id: rowCheckBox

                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    checkState: folderRow.checkState
                    enabled: !folderRow.accessDenied
                    focusPolicy: Qt.NoFocus
                    Accessible.ignored: true
                    onClicked: {
                        treeView.moveCurrentToRow(folderRow.row)
                        root.treeModel.toggleSelection(folderRow.treeIndex)
                    }
                }

                Item {
                    id: disclosure

                    anchors.left: rowCheckBox.right
                    anchors.leftMargin: folderRow.depth * IKSyncConfiguration.treeIndent
                    anchors.verticalCenter: parent.verticalCenter
                    width: IKSyncConfiguration.treeDisclosureSize
                    height: width

                    IKLoadingSpinner {
                        anchors.centerIn: parent
                        visible: folderRow.childrenLoading
                        width: IKSyncConfiguration.treeSpinnerSize
                        height: width
                        strokeWidth: 2
                        color: IKColors.syncConfigurationDisclosureIcon
                    }

                    AbstractButton {
                        anchors.fill: parent
                        visible: folderRow.disclosureVisible && !folderRow.childrenLoading
                        focusPolicy: Qt.NoFocus
                        Accessible.ignored: true
                        onClicked: {
                            if (folderRow.childrenLoadFailed) {
                                root.treeModel.retryChildren(folderRow.treeIndex)
                            } else {
                                folderRow.treeView.toggleExpanded(folderRow.row)
                            }
                        }

                        contentItem: Item {
                            IKTintedIcon {
                                anchors.centerIn: parent
                                visible: !folderRow.childrenLoadFailed
                                width: IKSyncConfiguration.treeChevronSize
                                height: width
                                rotation: folderRow.expanded ? 0 : -90
                                source: "qrc:/assets/main/chevron-down.svg"
                                color: IKColors.syncConfigurationDisclosureIcon
                            }

                            IKTintedIcon {
                                anchors.centerIn: parent
                                visible: folderRow.childrenLoadFailed
                                width: IKSyncConfiguration.treeRowIconSize
                                height: width
                                source: "qrc:/assets/main/triangle-alert.svg"
                                color: IKColors.statusMediumWarning
                            }
                        }
                    }
                }

                IKTintedIcon {
                    id: folderIcon

                    anchors.left: disclosure.right
                    anchors.leftMargin: IKSpacing.s4
                    anchors.verticalCenter: parent.verticalCenter
                    width: IKSyncConfiguration.treeRowIconSize
                    height: width
                    source: "qrc:/assets/main/folder.svg"
                    color: folderRow.accessDenied ? IKColors.actionDisabled : IKColors.syncConfigurationFolderIcon
                }

                Text {
                    id: folderNameText

                    anchors.left: folderIcon.right
                    anchors.leftMargin: IKSyncConfiguration.treeRowSpacing
                    anchors.right: sizeTextLabel.left
                    anchors.rightMargin: IKSyncConfiguration.treeRowSpacing
                    anchors.verticalCenter: parent.verticalCenter
                    text: folderRow.folderName
                    color: folderRow.accessDenied ? IKColors.actionDisabled : IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    elide: Text.ElideRight

                    IKToolTip {
                        visible: rowHover.hovered && folderNameText.truncated
                        text: folderRow.folderName
                        maximumTextWidth: IKSyncConfiguration.tooltipMaximumWidth
                    }
                }

                Text {
                    id: sizeTextLabel

                    width: IKSyncConfiguration.treeSizeColumnWidth
                    anchors.right: parent.right
                    anchors.rightMargin: IKSyncConfiguration.treeStateSpacing
                    anchors.verticalCenter: parent.verticalCenter
                    text: folderRow.sizeText
                    color: IKColors.textSecondary
                    font.pixelSize: IKFonts.subheadlineSize
                    horizontalAlignment: Text.AlignRight
                }
            }
        }
    }
}
