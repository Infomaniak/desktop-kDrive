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

ScrollView {
    id: root

    required property var controller
    readonly property string navigationTitle: qsTrId("filesToExclude")

    signal addRuleRequested(var trigger)

    contentWidth: availableWidth
    contentHeight: contentColumn.implicitHeight
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical.policy: ScrollBar.AsNeeded
    ScrollBar.vertical.interactive: true

    Component.onCompleted: controller.refresh()

    Column {
        id: contentColumn

        width: root.availableWidth
        padding: IKSettings.pageMargin
        spacing: IKSpacing.s24

        IKLoadingSpinner {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.controller.loading
            width: IKSettings.iconButtonSize
            height: width
            Accessible.name: qsTrId("filesToExclude")
        }

        Column {
            width: parent.width - 2 * IKSettings.pageMargin
            visible: root.controller.errorText.length > 0
            spacing: IKSpacing.s8

            Text {
                width: parent.width
                text: root.controller.errorText
                color: IKColors.statusStrongWarning
                font.pixelSize: IKFonts.bodySize
                wrapMode: Text.WordWrap
                Accessible.role: Accessible.AlertMessage
            }

            IKModalButton {
                visible: !root.controller.ready && !root.controller.loading
                text: qsTrId("buttonRetry")
                role: IKModalButton.Tonal
                onClicked: root.controller.refresh()
            }
        }

        Column {
            width: parent.width - 2 * IKSettings.pageMargin
            visible: root.controller.ready
            spacing: IKSpacing.s8

            Text {
                width: parent.width
                text: qsTrId("userExclusionFileListHeader")
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.emphasized
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: qsTrId("userExclusionFileListHeaderDescription")
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.subheadlineSize
                wrapMode: Text.WordWrap
            }

            IKModalButton {
                id: addRuleButton

                text: qsTrId("buttonAddFileExclusionRule")
                actionEnabled: root.controller.ready && !root.controller.saving
                onClicked: root.addRuleRequested(addRuleButton)
            }

            RowLayout {
                width: parent.width
                spacing: IKSpacing.s8

                IKCheckBox {
                    Layout.preferredWidth: implicitWidth
                    Layout.preferredHeight: implicitHeight
                    enabled: root.controller.ready && root.controller.userRuleCount > 0 && !root.controller.saving
                    checkState: root.controller.selectionCheckState
                    Accessible.name: qsTrId("labelRules")
                    onClicked: checkState === Qt.Checked ? root.controller.clearSelection() : root.controller.selectAll()
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTrId("labelRules")
                    color: IKColors.textSecondary
                    font.pixelSize: IKFonts.subheadlineSize
                }

                Text {
                    text: qsTrId("labelNotifyIfFileExcluded")
                    color: IKColors.textSecondary
                    font.pixelSize: IKFonts.subheadlineSize
                    horizontalAlignment: Text.AlignRight
                }
            }

            Rectangle {
                width: parent.width
                height: userRulesColumn.implicitHeight + 2 * IKSettings.groupPadding
                radius: IKRadius.r12
                color: IKColors.settingsCardSurface

                Column {
                    id: userRulesColumn

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: IKSettings.groupPadding
                    anchors.rightMargin: IKSettings.groupPadding

                    Text {
                        width: parent.width
                        visible: root.controller.userRuleCount === 0
                        text: qsTrId("noResultsFound")
                        color: IKColors.textSecondary
                        font.pixelSize: IKFonts.bodySize
                    }

                    Repeater {
                        model: root.controller.userRules

                        delegate: ExclusionRuleRow {
                            required property int index

                            width: userRulesColumn.width
                            controller: root.controller
                            row: index
                            editable: true
                        }
                    }
                }
            }

            Row {
                visible: root.controller.selectedCount > 0
                spacing: IKSpacing.s8

                IKModalButton {
                    text: qsTrId("buttonRemoveFileExclusionRule", root.controller.selectedCount)
                    role: IKModalButton.Destructive
                    actionEnabled: root.controller.ready && !root.controller.saving
                    busy: root.controller.saving
                    onClicked: root.controller.removeSelected()
                }

                IKModalButton {
                    text: qsTrId("buttonCancel")
                    role: IKModalButton.Tonal
                    actionEnabled: root.controller.ready && !root.controller.saving
                    onClicked: root.controller.clearSelection()
                }
            }
        }

        Column {
            width: parent.width - 2 * IKSettings.pageMargin
            visible: root.controller.ready
            spacing: IKSpacing.s8

            Text {
                width: parent.width
                text: qsTrId("defaultExclusionFileListHeader")
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.emphasized
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: qsTrId("defaultExclusionFileListDescription")
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.subheadlineSize
                wrapMode: Text.WordWrap
            }

            Rectangle {
                width: parent.width
                height: defaultRulesColumn.implicitHeight + 2 * IKSettings.groupPadding
                radius: IKRadius.r12
                color: IKColors.settingsCardSurface

                Column {
                    id: defaultRulesColumn

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: IKSettings.groupPadding
                    anchors.rightMargin: IKSettings.groupPadding

                    Text {
                        width: parent.width
                        visible: root.controller.defaultRules.count === 0
                        text: qsTrId("noResultsFound")
                        color: IKColors.textSecondary
                        font.pixelSize: IKFonts.bodySize
                    }

                    Repeater {
                        model: root.controller.defaultRules

                        delegate: ExclusionRuleRow {
                            required property int index

                            width: defaultRulesColumn.width
                            controller: root.controller
                            row: index
                        }
                    }
                }
            }
        }
    }
}
