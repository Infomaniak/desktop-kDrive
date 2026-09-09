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
    title: about ? qsTrId("aboutKDrive") : "kDrive " + controller.releaseVersion
    escapeDismissible: true
    preferredWidth: IKSettings.informationDialogWidth
    initialFocusItem: closeButton
    onDismissRequested: close()
    onClosed: {
        if (returnFocusItem && returnFocusItem.visible) {
            returnFocusItem.forceActiveFocus();
        }
    }
    function showFrom(item) {
        returnFocusItem = item;
        open();
    }
    bodyData: ScrollView {
        id: bodyScroll
        width: parent.width
        implicitHeight: Math.min(bodyColumn.implicitHeight, Math.max(IKSettings.logoSize, (root.parent ? root.parent.height : IKSettings.windowHeight) - IKSettings.dialogChromeHeight))
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
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.bodySize
                wrapMode: Text.WordWrap
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
            visible: !root.about && root.controller.updateAvailable
            text: qsTrId("buttonDownloadManually")
            external: true
            onClicked: root.controller.openDownload()
        }
    ]
}
