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

// "Errors to fix" page of the Activities tab: the errors of the selected synchronization, by section.
FocusScope {
    id: root

    required property var controller
    // Bounds of the dimmed background of the explanation dialog: the window surface, without its shadow.
    property real modalScrimInset: 0
    property real modalScrimRadius: 0

    readonly property bool hasErrors: root.controller.errorCount > 0
    // A refresh deletes the errors before detecting them again: the page keeps the list and its busy Refresh button
    // until the response, rather than showing "No errors to fix" for a moment.
    readonly property bool showsList: root.hasErrors || root.controller.refreshPending

    // Gives the keyboard focus to the main control of the page, once it is shown, or brings into view the row the
    // "Fix errors" action of an activity asked for.
    function focusDefault() {
        // Once the page has its size: the view positions its rows from it. The row is resolved only then, since the rows
        // or the selected synchronization can change meanwhile.
        Qt.callLater(root.revealOrFocus);
    }

    function revealOrFocus() {
        const revealRow = root.controller.takeRevealRow();
        if (revealRow < 0) {
            root.focusMainControl();
            return;
        }
        list.positionViewAtIndex(revealRow, ListView.Center);
        const rowItem = list.itemAtIndex(revealRow);
        if (rowItem) {
            rowItem.reveal();
        } else {
            root.focusMainControl();
        }
    }

    // Refresh is disabled while it runs: the page itself then takes the focus, so that it stays on a visible item.
    function focusMainControl() {
        if (!root.showsList) {
            seeActivitiesButton.forceActiveFocus();
        } else if (refreshButton.enabled) {
            refreshButton.forceActiveFocus();
        } else {
            focusFallback.forceActiveFocus();
        }
    }

    Item {
        id: focusFallback
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: IKActivities.pagePadding
        spacing: IKActivities.errorsCardSpacing
        visible: root.showsList

        RowLayout {
            Layout.fillWidth: true
            spacing: IKActivities.errorsCardActionSpacing

            Text {
                Layout.fillWidth: true
                // Empty while a refresh has no errors yet: 0 would read "one error" in French.
                text: root.hasErrors ? qsTrId("informationBlockSynchroErrorTitle", root.controller.errorCount) : ""
                textFormat: Text.StyledText
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                elide: Text.ElideRight
            }

            IKModalButton {
                id: refreshButton

                implicitHeight: IKActivities.errorsHeaderButtonHeight
                role: IKModalButton.Tonal
                text: qsTrId("buttonRefresh")
                busy: root.controller.refreshPending
                onClicked: root.controller.refresh()
            }
        }

        ListView {
            id: list

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: IKActivities.errorsCardSpacing
            boundsBehavior: Flickable.StopAtBounds
            // No current row: the view would keep it in place while the rows above settle their height, pushing the
            // first section title out of view. The keyboard goes through the row buttons instead.
            currentIndex: -1
            keyNavigationEnabled: false
            model: root.controller.model
            section.property: "sectionTitleId"
            section.delegate: Text {
                required property string section

                width: list.width
                topPadding: IKActivities.sectionSpacing - IKActivities.errorsCardSpacing
                bottomPadding: IKActivities.errorsSectionTitleSpacing - IKActivities.errorsCardSpacing
                text: qsTrId(section)
                textFormat: Text.PlainText
                color: IKColors.textPrimary
                font.pixelSize: IKActivities.errorsSectionTitleSize
                font.weight: IKFonts.emphasized
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }
            delegate: ErrorRow {
                width: ListView.view.width
                onActionTriggered: rowId => {
                    // The button keeps the focus back if the action opens the explanation dialog.
                    explanationDialog.returnFocusItem = root.Window.activeFocusItem;
                    root.controller.invokeAction(rowId);
                }
                onCopyRequested: rowId => root.controller.copyPath(rowId)
                onTechnicalDetailsCopyRequested: rowId => root.controller.copyTechnicalDetails(rowId)
            }

            ScrollBar.vertical: ScrollBar {}
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: IKActivities.emptyContentSpacing
        visible: !root.showsList

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            width: IKActivities.noActivityIllustrationWidth
            height: IKActivities.noActivityIllustrationHeight
            source: ThemeMode.isDark ? "qrc:/assets/main/activities/empty-dark.svg" : "qrc:/assets/main/activities/empty.svg"
            fillMode: Image.PreserveAspectFit
            sourceSize.width: width
            sourceSize.height: height
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTrId("labelNoErrorsToFix")
            color: IKColors.textPrimary
            font.pixelSize: IKActivities.emptyTitleSize
            font.weight: IKFonts.emphasized
            horizontalAlignment: Text.AlignHCenter
        }

        IKModalButton {
            id: seeActivitiesButton

            anchors.horizontalCenter: parent.horizontalCenter
            role: IKModalButton.Tonal
            text: qsTrId("buttonSeeActivities")
            onClicked: root.controller.showActivities()
        }
    }

    ErrorExplanationDialog {
        id: explanationDialog

        controller: root.controller.explanation
        scrimInset: root.modalScrimInset
        scrimRadius: root.modalScrimRadius
        onFallbackFocusRequested: root.focusDefault()
    }
}
