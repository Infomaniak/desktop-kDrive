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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import kDrive.UI

// "Choose the version to keep" dialog of a conflict. It follows `controller` (ConflictVersionController): it opens on
// the conflicts the page passes and closes once their choices are sent.
IKModal {
    id: root

    required property var controller

    // Restored when the dialog closes: the row button that opened it, set by the page.
    property Item returnFocusItem: null

    signal fallbackFocusRequested

    title: qsTrId("labelConflictDialogTitle")
    escapeDismissible: true
    preferredWidth: IKActivities.errorsConflictWidth
    initialFocusItem: validateButton.enabled ? validateButton : closeButton
    onDismissRequested: root.controller.close()
    // The page can go away while the dialog is open (sidebar, window closed): the next "Choose a version" must open it
    // again.
    Component.onDestruction: root.controller.close()
    onClosed: {
        root.controller.close();
        // The row button may be gone or busy (the resolution is running): the page then picks a usable control.
        if (root.returnFocusItem && root.returnFocusItem.visible && root.returnFocusItem.enabled) {
            root.returnFocusItem.forceActiveFocus();
        } else {
            root.fallbackFocusRequested();
        }
        root.returnFocusItem = null;
    }

    Connections {
        target: root.controller

        function onChanged() {
            // `visible` rather than `opened`, which stays false during the opening transition.
            if (root.controller.isOpen && !root.visible) {
                root.open();
            } else if (!root.controller.isOpen && root.visible) {
                root.close();
            }
        }
    }

    headerAccessoryData: Rectangle {
        // Only when several conflicts are reviewed in a row.
        visible: root.controller.count > 1
        implicitWidth: counterText.implicitWidth + 2 * IKActivities.errorsConflictCounterHorizontalPadding
        implicitHeight: counterText.implicitHeight + 2 * IKActivities.errorsConflictCounterVerticalPadding
        radius: height / 2
        color: IKColors.errorsConflictCounterSurface

        Text {
            id: counterText

            anchors.centerIn: parent
            text: qsTrId("labelXoverY").arg(root.controller.index + 1).arg(root.controller.count)
            textFormat: Text.PlainText
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.subheadlineSize
            font.weight: IKFonts.medium
        }
    }

    bodyData: ColumnLayout {
        id: content

        width: parent.width
        spacing: IKModalTokens.bodySpacing

        ErrorItemChip {
            Layout.maximumWidth: content.width
            visible: root.controller.itemName.length > 0
            name: root.controller.itemName
            path: ""
            iconName: root.controller.itemIconName
            isDirectory: false
        }

        Text {
            Layout.fillWidth: true
            text: qsTrId("labelConflictDialogQuestion")
            textFormat: Text.PlainText
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.medium
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: IKActivities.errorsConflictCardSpacing

            ConflictVersionCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                title: qsTrId("labelOnline")
                iconSource: "qrc:/assets/main/errors/cloud.svg"
                openText: qsTrId("buttonOpenInKDrive")
                authorName: root.controller.onlineAuthorName
                size: root.controller.onlineSize
                date: root.controller.onlineDate
                selected: root.controller.selection === ConflictVersionController.Online
                mostRecent: root.controller.mostRecent === ConflictVersionController.Online
                loading: root.controller.loading
                onSelectRequested: root.controller.select(ConflictVersionController.Online)
                onOpenRequested: root.controller.openVersion(ConflictVersionController.Online)
            }

            ConflictVersionCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                title: qsTrId("labelLocal")
                iconSource: "qrc:/assets/main/errors/computer.svg"
                openText: qsTrId("buttonOpenFile")
                authorName: root.controller.localAuthorName
                size: root.controller.localSize
                date: root.controller.localDate
                selected: root.controller.selection === ConflictVersionController.Local
                mostRecent: root.controller.mostRecent === ConflictVersionController.Local
                loading: root.controller.loading
                onSelectRequested: root.controller.select(ConflictVersionController.Local)
                onOpenRequested: root.controller.openVersion(ConflictVersionController.Local)
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.controller.failed
            text: qsTrId("unexpectedErrorTeachingTipContent")
            textFormat: Text.PlainText
            color: IKColors.statusStrongWarning
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.emphasized
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }
    }

    footerData: [
        IKModalButton {
            id: closeButton

            text: qsTrId("buttonClose")
            role: IKModalButton.Tonal
            onClicked: root.controller.close()
        },
        IKModalButton {
            id: validateButton

            text: root.controller.isLast ? qsTrId("buttonValidate") : qsTrId("buttonValidateAndGoNext")
            actionEnabled: root.controller.selection !== ConflictVersionController.None && !root.controller.loading
            busy: root.controller.resolutionPending
            onClicked: root.controller.validate()
        }
    ]
}
