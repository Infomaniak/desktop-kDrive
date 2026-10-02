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
import kDrive.UI

// "Sync a folder with kDrive" dialog: a form with the local folder and its kDrive location, whose second page picks that
// location in the remote folder tree.
IKModal {
    id: root

    required property var controller
    property Item returnFocusItem: null

    signal fallbackFocusRequested

    // One section of the form: a title, a description, the button choosing the folder, and the chosen folder.
    component FolderSection: Rectangle {
        id: section

        required property string title
        required property string description
        required property string buttonText
        required property string folderName
        required property string folderPath
        property string errorText: ""
        property bool buttonEnabled: true
        property alias button: chooseButton

        signal chooseRequested

        width: parent ? parent.width : implicitWidth
        implicitHeight: sectionContent.implicitHeight + 2 * IKSyncConfiguration.cardPadding
        radius: IKSyncConfiguration.cardRadius
        color: IKColors.syncConfigurationCardSurface

        Column {
            id: sectionContent

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: IKSyncConfiguration.cardPadding
            spacing: IKSyncConfiguration.cardSpacing

            Column {
                width: parent.width
                spacing: IKSyncConfiguration.cardTitleSpacing

                Text {
                    width: parent.width
                    text: section.title
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: IKFonts.emphasized
                    wrapMode: Text.WordWrap
                }

                Text {
                    width: parent.width
                    text: section.description
                    color: IKColors.textSecondary
                    font.pixelSize: IKFonts.subheadlineSize
                    wrapMode: Text.WordWrap
                }
            }

            Row {
                width: parent.width
                spacing: IKSyncConfiguration.fieldSpacing

                IKModalButton {
                    id: chooseButton

                    anchors.verticalCenter: parent.verticalCenter
                    role: IKModalButton.Tonal
                    text: section.buttonText
                    actionEnabled: section.buttonEnabled
                    onClicked: section.chooseRequested()
                }

                Rectangle {
                    id: folderChip

                    readonly property real contentPadding: IKSyncConfiguration.fieldHorizontalPadding
                    readonly property real iconSpace: IKSyncConfiguration.folderIconSize + IKSpacing.s4

                    // Sized from the name's own metrics, never from the elided label, so a long name cannot shrink it
                    // to nothing.
                    width: Math.min(nameText.implicitWidth + iconSpace + 2 * contentPadding,
                                    Math.max(0, parent.width - chooseButton.width - parent.spacing))
                    height: nameText.implicitHeight + 2 * IKSyncConfiguration.fieldVerticalPadding
                    anchors.verticalCenter: parent.verticalCenter
                    visible: section.folderName.length > 0
                    radius: IKSyncConfiguration.fieldRadius
                    color: IKColors.syncConfigurationFieldSurface
                    border.width: IKSyncConfiguration.fieldBorderWidth
                    border.color: IKColors.syncConfigurationFieldBorder
                    Accessible.role: Accessible.StaticText
                    Accessible.name: section.folderPath

                    HoverHandler {
                        id: chipHover
                    }

                    IKTintedIcon {
                        id: chipIcon

                        anchors.left: parent.left
                        anchors.leftMargin: folderChip.contentPadding
                        anchors.verticalCenter: parent.verticalCenter
                        width: IKSyncConfiguration.folderIconSize
                        height: width
                        source: "qrc:/assets/main/folder.svg"
                        color: IKColors.syncConfigurationFolderIcon
                    }

                    Text {
                        id: nameText

                        anchors.left: chipIcon.right
                        anchors.leftMargin: IKSpacing.s4
                        anchors.right: parent.right
                        anchors.rightMargin: folderChip.contentPadding
                        anchors.verticalCenter: parent.verticalCenter
                        text: section.folderName
                        textFormat: Text.PlainText
                        color: IKColors.textPrimary
                        font.pixelSize: IKFonts.bodySize
                        font.weight: IKFonts.emphasized
                        elide: Text.ElideRight
                    }

                    // The complete path, which also gives the complete name when it is elided.
                    IKToolTip {
                        showRequested: chipHover.hovered
                        text: section.folderPath
                        maximumTextWidth: IKSyncConfiguration.tooltipMaximumWidth
                    }
                }
            }

            SyncConfigurationErrorBlock {
                width: parent.width
                errorText: section.errorText
            }
        }
    }

    preferredWidth: IKSyncConfiguration.modalWidth
    escapeDismissible: !root.controller.busy
    visible: root.controller.visible
    title: root.controller.locationPickerOpen ? qsTrId("addAdvancedSyncRemoteFolderPageTitle")
                                              : qsTrId("addAdvancedSyncDialogTitle")
    initialFocusItem: localSection.button
    onDismissRequested: root.controller.cancelCurrentPage()
    onClosed: {
        if (root.returnFocusItem && root.returnFocusItem.enabled && root.returnFocusItem.visible) {
            root.returnFocusItem.forceActiveFocus(Qt.BacktabFocusReason);
        } else {
            root.fallbackFocusRequested();
        }
        root.returnFocusItem = null;
    }

    Connections {
        target: root.controller

        // The native picker is a separate window: the focus comes back to the button that opened it.
        function onLocalFolderDialogClosed() {
            Qt.callLater(function() {
                localSection.button.forceActiveFocus();
            });
        }
    }

    bodyData: [
        Column {
            width: parent ? parent.width : implicitWidth
            visible: !root.controller.locationPickerOpen
            spacing: IKSyncConfiguration.sectionSpacing

            // Back from the location picker: the focus returns to the button that opened it.
            onVisibleChanged: {
                if (visible && root.opened) {
                    Qt.callLater(function() {
                        remoteSection.button.forceActiveFocus();
                    });
                }
            }

            FolderSection {
                id: localSection

                title: qsTrId("addAdvancedSyncLocalFolderTitle")
                description: qsTrId("addAdvancedSyncLocalFolderDescription")
                buttonText: qsTrId("buttonSelectFolder")
                buttonEnabled: !root.controller.busy
                folderName: root.controller.localFolderName
                folderPath: root.controller.localPath
                errorText: root.controller.localFolderInvalid ? qsTrId("teachingTipInvalidFolderAdvancedContent") : ""
                onChooseRequested: root.controller.requestLocalFolder()
            }

            FolderSection {
                id: remoteSection

                title: qsTrId("addAdvancedSyncRemoteFolderTitle")
                description: qsTrId("addAdvancedSyncRemoteFolderDescription")
                buttonText: qsTrId("buttonSelectLocation")
                buttonEnabled: !root.controller.busy
                folderName: root.controller.remoteFolderName
                folderPath: root.controller.remotePath
                onChooseRequested: root.controller.openLocationPicker()
            }

            SyncConfigurationErrorBlock {
                width: parent.width
                errorText: root.controller.submitFailed ? qsTrId("unexpectedErrorTeachingTipContent") : ""
            }
        },
        Column {
            width: parent ? parent.width : implicitWidth
            visible: root.controller.locationPickerOpen
            spacing: IKSyncConfiguration.sectionSpacing

            // The tree is the only control of this page, so it takes the focus as soon as the page shows.
            onVisibleChanged: {
                if (visible) {
                    Qt.callLater(function() {
                        folderPicker.keyboardFocusItem.forceActiveFocus();
                    });
                }
            }

            Text {
                width: parent.width
                text: qsTrId("onboardingAdvancedSettingsDriveCustomizeLocationTip")
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.subheadlineSize
                wrapMode: Text.WordWrap
            }

            RemoteFolderPicker {
                id: folderPicker

                width: parent.width
                controller: root.controller
                treeModel: root.controller.pickerModel
                driveColor: root.controller.driveColor
            }

            SyncConfigurationErrorBlock {
                width: parent.width
                errorText: root.controller.folderCreationFailed
                           ? qsTrId("unableToCreateRemoteFolderTeachingTipTitle") + "\n"
                             + qsTrId("unableToCreateRemoteFolderTeachingTipContent")
                           : ""
            }
        }
    ]

    footerData: [
        IKModalButton {
            role: IKModalButton.Secondary
            text: qsTrId("buttonCancel")
            actionEnabled: !root.controller.busy
            onClicked: root.controller.cancelCurrentPage()
        },
        IKModalButton {
            role: IKModalButton.Primary
            text: root.controller.locationPickerOpen ? qsTrId("buttonSelect") : qsTrId("buttonValidate")
            actionEnabled: root.controller.locationPickerOpen ? root.controller.canConfirmLocation : root.controller.canSubmit
            busy: root.controller.submitting || root.controller.checkingLocalFolder
            onClicked: {
                if (root.controller.locationPickerOpen) {
                    root.controller.confirmLocation();
                } else {
                    root.controller.submit();
                }
            }
        }
    ]
}
