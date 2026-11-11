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

IKModal {
    id: root
    required property var controller
    property bool about: false
    property Item returnFocusItem: null
    // A check can announce another version while the dialog is open: its notes replace the previous ones.
    readonly property string announcedVersion: controller.releaseVersion
    onAnnouncedVersionChanged: {
        if (opened && !about) {
            controller.loadReleaseNotes();
        }
    }
    title: about ? qsTrId("aboutKDrive") : "kDrive " + controller.releaseVersion
    escapeDismissible: true
    // Wider for the release notes, so that fewer of their items wrap.
    preferredWidth: about ? IKSettings.informationDialogWidth : IKSettings.releaseNotesDialogWidth
    initialFocusItem: closeButton
    onDismissRequested: close()
    onClosed: {
        if (returnFocusItem && returnFocusItem.visible) {
            returnFocusItem.forceActiveFocus();
        }
    }
    function showFrom(item) {
        returnFocusItem = item;
        if (!about) {
            controller.loadReleaseNotes();
        }
        open();
    }
    bodyData: ScrollView {
        id: bodyScroll
        width: parent.width
        // The overlay spans the transparent shadow margin: keep the dialog inside the visible window surface.
        readonly property real visibleHeight: (root.parent ? root.parent.height : IKSettings.windowHeight) - 2 * root.scrimInset
        implicitHeight: Math.min(bodyColumn.implicitHeight,
                                 Math.max(IKSettings.logoSize,
                                          visibleHeight - 2 * IKModalTokens.screenMargin - IKSettings.dialogChromeHeight))
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        Column {
            id: bodyColumn
            width: bodyScroll.availableWidth
            spacing: IKSpacing.s16
            Image {
                width: IKSettings.logoSize
                height: width
                source: "qrc:/assets/taskbar/logo_kdrive.svg"
                fillMode: Image.PreserveAspectFit
                sourceSize.width: width * 3
            }
            Text {
                width: parent.width
                visible: root.about
                text: qsTrId("aboutKDriveDescription")
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                wrapMode: Text.WordWrap
            }
            Text {
                width: parent.width
                text: root.about ? root.controller.installedDetails : root.controller.releaseDetails
                // Escaped by the controller, which sets the version and build in bold.
                textFormat: Text.StyledText
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.bodySize
                wrapMode: Text.WordWrap
            }
            // Release notes of the announced version, hidden when they cannot be loaded.
            Column {
                id: releaseNotesColumn
                width: parent.width
                visible: !root.about && root.controller.releaseNotes.length > 0
                spacing: IKSpacing.s12
                Repeater {
                    model: root.controller.releaseNotes
                    Column {
                        id: releaseNotesSection
                        required property var modelData
                        // Repeater delegates lose their parent while being destroyed: bind to ids, not to parent.
                        width: releaseNotesColumn.width
                        spacing: IKSpacing.s4
                        Text {
                            width: releaseNotesSection.width
                            visible: text.length > 0
                            text: releaseNotesSection.modelData.title
                            textFormat: Text.PlainText
                            color: IKColors.textPrimary
                            font.pixelSize: IKFonts.bodySize
                            font.weight: IKFonts.emphasized
                            wrapMode: Text.WordWrap
                        }
                        Repeater {
                            model: releaseNotesSection.modelData.items
                            Text {
                                required property string modelData
                                width: releaseNotesSection.width
                                text: "• " + modelData
                                textFormat: Text.PlainText
                                color: IKColors.textSecondary
                                font.pixelSize: IKFonts.bodySize
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
            Text {
                width: parent.width
                visible: text.length > 0
                text: root.controller.errorText
                color: IKColors.statusStrongWarning
                font.pixelSize: IKFonts.bodySize
                wrapMode: Text.WordWrap
                Accessible.role: Accessible.AlertMessage
            }
            Flow {
                width: parent.width
                visible: root.about
                spacing: IKSpacing.s12
                IKLinkButton {
                    text: "GPL v3"
                    external: true
                    onClicked: root.controller.openLicense()
                }
                IKLinkButton {
                    text: qsTrId("viewSourceCode")
                    external: true
                    onClicked: root.controller.openSources()
                }
            }
        }
    }
    footerData: [
        IKModalButton {
            id: closeButton
            text: qsTrId("buttonClose")
            role: IKModalButton.Secondary
            onClicked: root.close()
        },
        IKModalButton {
            readonly property bool manual: !root.controller.updateReadyToInstall && !root.controller.updateDownloading
                                           && !root.controller.updateInstallable

            visible: !root.about && root.controller.updateAvailable
            text: root.controller.updateReadyToInstall ? qsTrId("buttonRestart")
                                                       : root.controller.updateDownloading ? qsTrId("buttonDownloadInProgress")
                                                       : root.controller.updateInstallable ? qsTrId("buttonUpdate")
                                                                                           : qsTrId("buttonDownloadManually")
            external: manual
            actionEnabled: !root.controller.updateDownloading
            onClicked: manual ? root.controller.openDownload() : root.controller.installUpdate()
        }
    ]
}
