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
import QtQuick.Layouts
import kDrive.UI

// "Understand the problem" dialog of the errors page. It follows `controller` (SyncErrorExplanationController): it opens
// for one error and closes with it. The texts of each explanation are chosen here; the controller only gives the kind.
IKModal {
    id: root

    required property var controller

    readonly property int kind: root.controller.kind
    readonly property bool isFolderNotFound: root.kind === SyncErrors.SyncDirNotFound
    // Reasons shown in boxes: title, advice, and the link the advice holds ("faq", "newSync") or none.
    readonly property var reasons: {
        if (root.isFolderNotFound) {
            return [
                {
                    "titleId": "labelFolderMovedOrRenamed",
                    "adviceId": "labelRestoreToOriginalLocation",
                    "linkId": "",
                    "link": ""
                },
                {
                    "titleId": "labelAccessRightsModified",
                    "adviceId": "errDialogLocalFileAccessFaqLink",
                    "linkId": "labelFAQ",
                    "link": "faq"
                },
                {
                    "titleId": "labelFolderDeleted",
                    "adviceId": "labelCreateNewSync",
                    "linkId": "labelNewSync",
                    "link": "newSync"
                }
            ];
        }
        if (root.kind === SyncErrors.SyncDirChangedRemotely) {
            return [
                {
                    "titleId": "labelOnlineFolderDeletedOrRenamed",
                    "adviceId": "labelOnlineFolderDeletedOrRenamedTip",
                    "linkId": "",
                    "link": ""
                },
                {
                    "titleId": "labelAccessRightsModified",
                    "adviceId": "linuxLabelRemoteAccessRightsModifiedTip",
                    "linkId": "",
                    "link": ""
                },
                {
                    "titleId": "labelFolderRecreatedOrReplaced",
                    "adviceId": "labelFolderRecreatedOrReplacedTip",
                    "linkId": "",
                    "link": ""
                }
            ];
        }
        if (root.kind === SyncErrors.SyncDirChangedLocally) {
            return [
                {
                    "titleId": "labelNewDeviceDetected",
                    "adviceId": "labelNewDeviceDetectedTip",
                    "linkId": "",
                    "link": ""
                },
                {
                    "titleId": "labelFolderDisplacedOrModified",
                    "adviceId": "labelFolderDisplacedOrModifiedTip",
                    "linkId": "",
                    "link": ""
                },
                {
                    "titleId": "labelFolderRecreatedOrReplaced",
                    "adviceId": "labelFolderRecreatedOrReplacedTip",
                    "linkId": "",
                    "link": ""
                }
            ];
        }
        return [];
    }
    // Restored when the dialog closes: the row button that opened it, set by the page.
    property Item returnFocusItem: null

    signal fallbackFocusRequested

    function nodeText(id) {
        return ErrorTexts.text(id, 1, root.controller.nodeLabelId, 0);
    }

    function followLink(link) {
        if (link === "faq") {
            // TODO Point to an article on restoring access rights once it exists: `faqUrl` is the FAQ home page, which
            // does not answer it.
            Qt.openUrlExternally(qsTrId("faqUrl"));
        } else if (link === "newSync") {
            root.controller.createNewSync();
        }
    }

    title: {
        switch (root.kind) {
        case SyncErrors.LocalAccess:
            return root.nodeText("errLocalFileAccessTitle");
        case SyncErrors.SyncDirNotFound:
            return qsTrId("errSystemErrorSyncDirAccessTitle");
        case SyncErrors.SyncDirDiskMissing:
            return qsTrId("errDialogSystemSyncDirDiskMissingTitle");
        case SyncErrors.SyncDirChangedLocally:
        case SyncErrors.SyncDirChangedRemotely:
            return qsTrId("errDialogConfigChangedTitle");
        case SyncErrors.INotifyWatches:
            return qsTrId("linuxErrINotifyWatchesTitle");
        default:
            return "";
        }
    }
    escapeDismissible: true
    preferredWidth: IKActivities.errorsExplanationWidth
    initialFocusItem: mainButton
    onDismissRequested: root.controller.close()
    // The page can go away while the dialog is open (sidebar, window closed): the next "Understand the problem" must
    // open it again.
    Component.onDestruction: root.controller.close()
    // A failed "Raise the limit" can open the controller while the page is away; the dialog shows on its return.
    Component.onCompleted: {
        if (root.controller.isOpen) {
            root.open();
        }
    }
    onClosed: {
        root.controller.close();
        // The row button may be gone or busy (its action is running): the page then picks a usable control.
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

    bodyData: ScrollView {
        id: bodyScroll

        width: parent.width
        implicitHeight: Math.min(bodyColumn.implicitHeight, Math.max(0, (root.parent ? root.parent.height : bodyColumn.implicitHeight)
                                                                         - IKActivities.errorsExplanationChromeHeight))
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: bodyColumn

            width: bodyScroll.availableWidth
            spacing: IKActivities.errorsExplanationReasonSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                ErrorLinkText {
                    Layout.fillWidth: true
                    sentence: {
                        switch (root.kind) {
                        case SyncErrors.LocalAccess:
                            return root.nodeText("errDialogLocalFileAccessDescription") + " %1";
                        case SyncErrors.SyncDirDiskMissing:
                            return qsTrId("dialogSystemErrorSyncDirDiskMissingErrorDescription");
                        case SyncErrors.SyncDirChangedLocally:
                        case SyncErrors.SyncDirChangedRemotely:
                            return qsTrId("dialogDataErrorSyncDirChangeDescription");
                        case SyncErrors.INotifyWatches:
                            return qsTrId("linuxErrINotifyWatchesDescription");
                        default:
                            return qsTrId("dialogSystemErrorSyncDirAccessErrorDescription");
                        }
                    }
                    linkText: root.kind === SyncErrors.LocalAccess
                              ? qsTrId("errDialogLocalFileAccessFaqLink").arg(qsTrId("labelFAQ")) : ""
                    onActivated: root.followLink("faq")
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.kind === SyncErrors.SyncDirDiskMissing
                    text: qsTrId("dialogSystemErrorSyncDirDiskMissingErrorTip")
                    textFormat: Text.PlainText
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: IKFonts.emphasized
                    wrapMode: Text.WordWrap
                }
            }

            ErrorItemChip {
                Layout.maximumWidth: bodyColumn.width
                visible: root.kind === SyncErrors.LocalAccess && root.controller.itemName.length > 0
                name: root.controller.itemName
                path: root.controller.itemPath
                iconName: root.controller.itemIconName
                isDirectory: root.controller.isDirectory
            }

            Repeater {
                model: root.reasons

                delegate: Rectangle {
                    id: reason

                    required property var modelData

                    Layout.fillWidth: true
                    implicitHeight: reasonContent.implicitHeight + 2 * IKActivities.errorsExplanationReasonPadding
                    radius: IKActivities.errorsExplanationReasonRadius
                    color: IKColors.errorsExplanationReasonSurface

                    ColumnLayout {
                        id: reasonContent

                        anchors.fill: parent
                        anchors.margins: IKActivities.errorsExplanationReasonPadding
                        spacing: IKActivities.errorsExplanationReasonContentSpacing

                        Text {
                            Layout.fillWidth: true
                            text: qsTrId(reason.modelData.titleId)
                            textFormat: Text.PlainText
                            color: IKColors.textPrimary
                            font.pixelSize: IKFonts.bodySize
                            font.weight: IKFonts.emphasized
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: IKActivities.errorsExplanationArrowSpacing

                            FontMetrics {
                                id: adviceMetrics

                                font: adviceText.font
                            }

                            // Centered on the first line of the advice, which can wrap.
                            IKTintedIcon {
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: Math.max(0, (adviceMetrics.height - height) / 2)
                                width: IKActivities.errorsExplanationArrowSize
                                height: width
                                source: "qrc:/assets/main/arrow-right.svg"
                                color: IKColors.errorsExplanationArrow
                            }

                            ErrorLinkText {
                                id: adviceText

                                Layout.fillWidth: true
                                sentence: qsTrId(reason.modelData.adviceId)
                                linkText: reason.modelData.linkId.length > 0 ? qsTrId(reason.modelData.linkId) : ""
                                onActivated: root.followLink(reason.modelData.link)
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.kind === SyncErrors.LocalAccess || root.isFolderNotFound
                spacing: IKActivities.errorsExplanationReasonContentSpacing

                Text {
                    Layout.fillWidth: true
                    text: root.kind === SyncErrors.LocalAccess ? root.nodeText("errDialogLocalFileAccessLocationLabel")
                                                               : qsTrId("labelOriginalLocation")
                    textFormat: Text.PlainText
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: root.isFolderNotFound ? IKFonts.emphasized : IKFonts.regular
                    wrapMode: Text.WordWrap
                }

                ErrorCopyablePath {
                    Layout.fillWidth: true
                    value: root.controller.copyablePath
                    onCopyRequested: root.controller.copyPath()
                }
            }

            // Inotify watches: the current limit and the command that raises it, run by the user in a terminal.
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.kind === SyncErrors.INotifyWatches
                spacing: IKActivities.errorsExplanationReasonContentSpacing

                // Why the dialog replaces "Raise the limit".
                Text {
                    Layout.fillWidth: true
                    visible: root.controller.raiseWatchLimitCancelled || root.controller.raiseWatchLimitFailed
                    text: root.controller.raiseWatchLimitCancelled ? qsTrId("linuxDialogINotifyWatchesRaiseCancelled")
                                                                   : qsTrId("linuxDialogINotifyWatchesRaiseFailed")
                    textFormat: Text.PlainText
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.controller.watchLimit > 0
                    text: qsTrId("linuxDialogINotifyWatchesCurrentLimit").arg(root.controller.watchLimit.toLocaleString(Qt.locale(), "f", 0))
                    textFormat: Text.PlainText
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTrId("linuxDialogINotifyWatchesCommand")
                    textFormat: Text.PlainText
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: IKFonts.emphasized
                    wrapMode: Text.WordWrap
                }

                ErrorCopyablePath {
                    Layout.fillWidth: true
                    value: root.controller.watchLimitCommand
                    wrapsValue: true
                    copyText: qsTrId("linuxButtonCopyCommand")
                    copiedText: qsTrId("linuxCommandCopiedToClipboard")
                    onCopyRequested: root.controller.copyWatchLimitCommand()
                }
            }
        }
    }

    footerData: [
        IKModalButton {
            text: qsTrId("buttonClose")
            role: IKModalButton.Tonal
            onClicked: root.controller.close()
        },
        IKModalButton {
            id: mainButton

            text: {
                switch (root.kind) {
                case SyncErrors.SyncDirDiskMissing:
                case SyncErrors.INotifyWatches:
                    return qsTrId("buttonRestartSync");
                case SyncErrors.SyncDirChangedLocally:
                case SyncErrors.SyncDirChangedRemotely:
                    return qsTrId("buttonCreateNewSync");
                default:
                    return qsTrId("buttonOpenParentFolder");
                }
            }
            onClicked: root.controller.triggerMainAction()
        }
    ]
}
