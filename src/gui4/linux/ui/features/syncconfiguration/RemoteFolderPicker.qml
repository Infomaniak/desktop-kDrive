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

// Remote destination picker: a lazy folder tree with a single selection, rooted at the drive, where a folder can be
// created below any folder that accepts one. The model owns the selection and the editing row; the controller creates
// the folder.
Rectangle {
    id: root

    required property var controller
    required property var treeModel
    required property color driveColor

    // The tree itself takes the keyboard focus; the surrounding frame is decoration.
    readonly property Item keyboardFocusItem: treeView
    readonly property bool contentVisible: !treeModel.loading && !treeModel.loadFailed
    property bool rootExpanded: false
    // Folder under the keyboard cursor. The cursor is drawn from this id rather than from the delegates' `current`, which
    // TreeView does not refresh when rows are inserted or removed above the current index: two rows then look current.
    property string cursorNodeId: ""

    implicitHeight: IKSyncConfiguration.treeHeight
    radius: IKSyncConfiguration.treeRadius
    color: IKColors.syncConfigurationTreeSurface
    border.width: IKSyncConfiguration.treeBorderWidth
    border.color: IKColors.syncConfigurationDivider

    Connections {
        target: root.treeModel

        // The drive root is expanded whenever the tree is configured again.
        function onModelReset() {
            root.rootExpanded = false;
            root.cursorNodeId = "";
        }

        // The editing row can land in a collapsed folder or outside the viewport: it is shown before it takes the focus.
        function onFolderCreationStarted(parentIndex) {
            const parentRow = treeView.rowAtIndex(parentIndex);
            if (parentRow >= 0) {
                treeView.expand(parentRow);
            }
            Qt.callLater(function() {
                const editingRow = treeView.rowAtIndex(root.treeModel.index(0, 0, parentIndex));
                if (editingRow >= 0) {
                    treeView.positionViewAtRow(editingRow, TableView.Contain);
                }
            });
        }

        // The created folder is sorted among its siblings, possibly outside the viewport: the cursor follows it.
        function onFolderCreated(index) {
            Qt.callLater(function() {
                const createdRow = treeView.rowAtIndex(index);
                if (createdRow >= 0) {
                    treeView.forceActiveFocus();
                    treeView.moveCurrentToRow(createdRow);
                }
            });
        }
    }

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

    TreeView {
        id: treeView

        // The view row of the current index, which follows its folder when rows move, unlike `currentRow`.
        function cursorRow(): int {
            return treeView.rowAtIndex(treeSelection.currentIndex);
        }

        // A single tab stop moving a current row internally, like the selective synchronization tree.
        function moveCurrentToRow(targetRow: int): void {
            if (targetRow < 0 || targetRow >= treeView.rows) {
                return;
            }
            treeSelection.setCurrentIndex(treeView.index(targetRow, 0), ItemSelectionModel.NoUpdate);
            treeView.positionViewAtRow(targetRow, TableView.Contain);
        }

        function placeInitialCurrentRow(): void {
            if (treeView.activeFocus && treeView.cursorRow() < 0) {
                treeView.moveCurrentToRow(0);
            }
        }

        function expandDriveRoot(): void {
            if (!root.rootExpanded && treeView.rows > 0) {
                root.rootExpanded = true;
                treeView.expand(0);
            }
        }

        anchors.fill: parent
        anchors.margins: IKSyncConfiguration.treeContentPadding
        visible: root.contentVisible
        clip: true
        model: root.treeModel
        boundsBehavior: Flickable.StopAtBounds
        activeFocusOnTab: true
        keyNavigationEnabled: false
        selectionModel: ItemSelectionModel {
            id: treeSelection

            model: root.treeModel
            onCurrentChanged: current => root.cursorNodeId = root.treeModel.nodeIdAt(current)
        }

        onActiveFocusChanged: treeView.placeInitialCurrentRow()
        onRowsChanged: {
            treeView.expandDriveRoot();
            treeView.placeInitialCurrentRow();
        }

        Keys.onUpPressed: treeView.moveCurrentToRow(treeView.cursorRow() - 1)
        Keys.onDownPressed: treeView.moveCurrentToRow(treeView.cursorRow() + 1)
        Keys.onLeftPressed: {
            const row = treeView.cursorRow();
            if (row < 0) {
                return;
            }
            if (treeView.isExpanded(row)) {
                treeView.collapse(row);
                return;
            }
            treeView.moveCurrentToRow(treeView.rowAtIndex(root.treeModel.parentIndex(treeSelection.currentIndex)));
        }
        Keys.onRightPressed: {
            const row = treeView.cursorRow();
            if (row < 0) {
                return;
            }
            if (!treeView.isExpanded(row)) {
                treeView.expand(row);
                return;
            }
            treeView.moveCurrentToRow(row + 1);
        }
        Keys.onSpacePressed: root.treeModel.select(treeSelection.currentIndex)
        Keys.onReturnPressed: root.treeModel.select(treeSelection.currentIndex)
        Keys.onEnterPressed: root.treeModel.select(treeSelection.currentIndex)

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
            required property string nodeId
            required property string folderName
            required property bool driveRoot
            required property bool selectable
            required property bool canCreateFolder
            required property bool unavailable
            required property bool accessDenied
            required property bool folderSelected
            required property bool childrenLoading
            required property bool childrenLoadFailed
            required property bool nameEditing
            readonly property bool isCursor: folderRow.nodeId !== "" && folderRow.nodeId === root.cursorNodeId

            readonly property bool dimmed: folderRow.unavailable || folderRow.accessDenied
            readonly property bool creationPending: folderRow.nameEditing && root.treeModel.folderCreationPending
            // Folder reported visible to the model, empty while the row is pooled.
            property string registeredNodeId: ""
            // Set while the view keeps the delegate aside for reuse.
            property bool pooled: false

            // Resolved when used, never kept: after an expansion, a row can show another folder without its `row`
            // changing.
            function currentTreeIndex(): var {
                return folderRow.treeView.index(folderRow.row, folderRow.column);
            }

            function registerVisibleNode(): void {
                folderRow.registeredNodeId = folderRow.nodeId;
                root.treeModel.setNodeVisible(folderRow.registeredNodeId, true);
            }

            function unregisterVisibleNode(): void {
                if (folderRow.registeredNodeId === "") {
                    return;
                }
                root.treeModel.setNodeVisible(folderRow.registeredNodeId, false);
                folderRow.registeredNodeId = "";
            }

            // Choosing another folder abandons a folder creation that was not sent yet. The selection is made first, while
            // the row still designates this folder, since dropping the editing row can shift the rows below it.
            function requestSelection(): void {
                treeView.moveCurrentToRow(folderRow.row);
                root.treeModel.select(folderRow.currentTreeIndex());
                if (root.treeModel.folderCreationActive && !root.treeModel.folderCreationPending) {
                    root.controller.cancelFolderCreation();
                }
            }

            function focusNameField(): void {
                if (folderRow.nameEditing) {
                    nameField.text = "";
                    nameField.forceActiveFocus();
                }
            }

            implicitWidth: folderRow.treeView.width
            implicitHeight: IKSyncConfiguration.treeRowHeight

            Accessible.role: Accessible.TreeItem
            Accessible.name: folderRow.folderName
            Accessible.selectable: folderRow.selectable
            Accessible.selected: folderRow.folderSelected
            Accessible.readOnly: !folderRow.selectable

            Component.onCompleted: {
                folderRow.registerVisibleNode();
                folderRow.focusNameField();
            }
            Component.onDestruction: folderRow.unregisterVisibleNode()
            TableView.onPooled: {
                folderRow.pooled = true;
                folderRow.unregisterVisibleNode();
            }
            TableView.onReused: {
                folderRow.pooled = false;
                folderRow.registerVisibleNode();
                folderRow.focusNameField();
            }
            onNameEditingChanged: folderRow.focusNameField()

            // A delegate can stay on the editing row from one creation to the next: each one starts from an empty name.
            Connections {
                target: root.treeModel

                function onFolderCreationStarted() {
                    folderRow.focusNameField();
                }
            }
            onNodeIdChanged: {
                if (folderRow.registeredNodeId === "") {
                    return;
                }
                folderRow.unregisterVisibleNode();
                folderRow.registerVisibleNode();
            }

            Rectangle {
                anchors.fill: parent
                radius: IKSyncConfiguration.treeRowRadius
                color: {
                    if (folderRow.folderSelected) {
                        return IKColors.remoteFolderPickerRowSelected;
                    }
                    if (folderRow.isCursor && folderRow.treeView.activeFocus) {
                        return IKColors.syncConfigurationRowCurrent;
                    }
                    if (rowHover.hovered && folderRow.selectable) {
                        return IKColors.syncConfigurationRowHover;
                    }
                    return "transparent";
                }
            }

            // Disabled while pooled: a delegate put aside under the cursor would otherwise keep reporting a hover once reused
            // for another row, since it no longer receives the pointer leaving it.
            HoverHandler {
                id: rowHover

                enabled: !folderRow.pooled
            }

            TapHandler {
                enabled: !folderRow.nameEditing
                onTapped: folderRow.requestSelection()
            }

            Item {
                id: disclosure

                anchors.left: parent.left
                anchors.leftMargin: folderRow.depth * IKSyncConfiguration.treeIndent
                anchors.verticalCenter: parent.verticalCenter
                width: IKSyncConfiguration.treeDisclosureSize
                height: width

                IKLoadingSpinner {
                    anchors.centerIn: parent
                    visible: folderRow.childrenLoading || folderRow.creationPending
                    width: IKSyncConfiguration.treeSpinnerSize
                    height: width
                    strokeWidth: 2
                    color: IKColors.syncConfigurationDisclosureIcon
                }

                AbstractButton {
                    anchors.fill: parent
                    visible: folderRow.isTreeNode && folderRow.hasChildren && !folderRow.childrenLoading
                    focusPolicy: Qt.NoFocus
                    Accessible.ignored: true
                    onClicked: {
                        if (folderRow.childrenLoadFailed) {
                            root.treeModel.retryChildren(folderRow.currentTreeIndex());
                        } else {
                            folderRow.treeView.toggleExpanded(folderRow.row);
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

            Item {
                id: folderIcon

                anchors.left: disclosure.right
                anchors.leftMargin: IKSpacing.s4
                anchors.verticalCenter: parent.verticalCenter
                width: IKSyncConfiguration.treeRowIconSize
                height: width

                DriveIconView {
                    anchors.fill: parent
                    visible: folderRow.driveRoot
                    iconColor: root.driveColor
                }

                IKTintedIcon {
                    anchors.fill: parent
                    visible: !folderRow.driveRoot
                    source: "qrc:/assets/main/folder.svg"
                    color: folderRow.dimmed ? IKColors.actionDisabled : IKColors.syncConfigurationFolderIcon
                }
            }

            Text {
                id: folderNameText

                anchors.left: folderIcon.right
                anchors.leftMargin: IKSyncConfiguration.treeRowSpacing
                anchors.right: createFolderButton.left
                anchors.rightMargin: IKSyncConfiguration.treeRowSpacing
                anchors.verticalCenter: parent.verticalCenter
                visible: !folderRow.nameEditing
                text: folderRow.folderName
                textFormat: Text.PlainText
                color: folderRow.dimmed ? IKColors.actionDisabled : IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                font.weight: folderRow.driveRoot ? IKFonts.emphasized : Font.Normal
                elide: Text.ElideRight

                IKToolTip {
                    showRequested: rowHover.hovered && (folderRow.unavailable || folderNameText.truncated)
                    text: folderRow.unavailable ? qsTrId("errorSelectedFolderIncorrect") : folderRow.folderName
                    maximumTextWidth: IKSyncConfiguration.tooltipMaximumWidth
                }
            }

            TextField {
                id: nameField

                anchors.left: folderIcon.right
                anchors.leftMargin: IKSyncConfiguration.treeRowSpacing
                anchors.right: createFolderButton.left
                anchors.rightMargin: IKSyncConfiguration.treeRowSpacing
                anchors.verticalCenter: parent.verticalCenter
                height: folderRow.height - 2 * IKSpacing.s4
                visible: folderRow.nameEditing
                readOnly: folderRow.creationPending
                placeholderText: qsTrId("labelNewFolder")
                color: IKColors.textPrimary
                placeholderTextColor: IKColors.textTertiary
                font.pixelSize: IKFonts.bodySize
                selectByMouse: true
                leftPadding: IKSpacing.s8
                rightPadding: IKSpacing.s8
                topPadding: 0
                bottomPadding: 0
                verticalAlignment: TextInput.AlignVCenter
                Accessible.name: qsTrId("labelNewFolder")
                Keys.onReturnPressed: root.controller.commitFolderCreation(nameField.text)
                Keys.onEnterPressed: root.controller.commitFolderCreation(nameField.text)
                Keys.onEscapePressed: event => {
                    event.accepted = true;
                    root.controller.cancelFolderCreation();
                    treeView.forceActiveFocus();
                }

                background: Rectangle {
                    radius: IKRadius.r6
                    color: IKColors.surfacePrimary
                    border.width: nameField.activeFocus ? 2 : 1
                    border.color: nameField.activeFocus ? IKColors.accentPrimary : IKColors.settingsDivider
                }
            }

            AbstractButton {
                id: createFolderButton

                anchors.right: parent.right
                anchors.rightMargin: IKSyncConfiguration.treeStateSpacing
                anchors.verticalCenter: parent.verticalCenter
                width: IKSyncConfiguration.treeDisclosureSize
                height: width
                // Shown on the hovered or current row, as in the reference design.
                opacity: folderRow.canCreateFolder && !root.treeModel.folderCreationPending
                         && (rowHover.hovered || (folderRow.isCursor && folderRow.treeView.activeFocus)) ? 1 : 0
                enabled: opacity > 0
                focusPolicy: Qt.NoFocus
                hoverEnabled: true
                text: qsTrId("labelNewFolder")
                Accessible.name: text + " " + folderRow.folderName
                onClicked: {
                    treeView.moveCurrentToRow(folderRow.row);
                    root.controller.beginFolderCreation(folderRow.currentTreeIndex());
                }

                contentItem: Item {
                    IKTintedIcon {
                        anchors.centerIn: parent
                        width: IKSyncConfiguration.treeRowIconSize
                        height: width
                        source: "qrc:/assets/settings/folder-circle-plus.svg"
                        color: IKColors.remoteFolderPickerCreateIcon
                    }
                }

                background: null

                IKToolTip {
                    targetButton: createFolderButton
                    text: createFolderButton.text
                }
            }
        }
    }
}
