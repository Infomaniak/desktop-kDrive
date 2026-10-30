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

// "Version conflicts" page: one strategy applied to every conflict of the selected synchronization, or the list of
// conflicts to process them one by one.
FocusScope {
    id: root

    required property var controller

    readonly property string countText: root.controller.count.toLocaleString(Qt.locale(), "f", 0)
    readonly property var strategies: [
        {
            "strategy": ConflictsPageController.MostRecent,
            "titleId": "labelConflictStrategyKeepMostRecentTitle",
            "descriptionId": "labelConflictStrategyKeepMostRecentDescription",
            "descriptionHasCount": false,
            "iconSource": "qrc:/assets/main/errors/clock.svg",
            "recommended": true
        },
        {
            "strategy": ConflictsPageController.Online,
            "titleId": "labelConflictStrategyKeepRemoteTitle",
            "descriptionId": "labelConflictStrategyKeepRemoteDescription",
            "descriptionHasCount": true,
            "iconSource": "qrc:/assets/main/errors/cloud.svg",
            "recommended": false
        },
        {
            "strategy": ConflictsPageController.Local,
            "titleId": "labelConflictStrategyKeepLocalTitle",
            "descriptionId": "labelConflictStrategyKeepLocalDescription",
            "descriptionHasCount": true,
            "iconSource": "qrc:/assets/main/errors/computer.svg",
            "recommended": false
        }
    ]

    function focusDefault() {
        strategyGroup.forceActiveFocus();
    }

    // Up and down move the selection inside the group, like native radio buttons.
    function moveSelection(step) {
        const current = root.strategies.findIndex(entry => entry.strategy === root.controller.strategy);
        const next = Math.max(0, Math.min(root.strategies.length - 1, current + step));
        if (next !== current) {
            root.controller.selectStrategy(root.strategies[next].strategy);
        }
    }

    Flickable {
        id: flickable

        anchors.fill: parent
        contentWidth: width
        contentHeight: content.implicitHeight + 2 * IKActivities.pagePadding
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: content

            x: IKActivities.pagePadding
            y: IKActivities.pagePadding
            width: flickable.width - 2 * IKActivities.pagePadding
            spacing: IKActivities.conflictsSectionSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: IKActivities.conflictsHeaderSpacing

                Text {
                    Layout.fillWidth: true
                    text: qsTrId("labelManyConflictTitle").arg(root.countText)
                    textFormat: Text.PlainText
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTrId("labelChooseConflictVersion")
                    textFormat: Text.PlainText
                    color: IKColors.textTertiary
                    font.pixelSize: IKFonts.subheadlineSize
                    wrapMode: Text.WordWrap
                }
            }

            // A single tab stop: the selected card holds the focus inside the group.
            FocusScope {
                id: strategyGroup

                Layout.fillWidth: true
                implicitHeight: strategyColumn.implicitHeight
                activeFocusOnTab: true
                Accessible.role: Accessible.Grouping
                Accessible.name: qsTrId("labelChooseConflictVersion")
                Keys.onUpPressed: root.moveSelection(-1)
                Keys.onDownPressed: root.moveSelection(1)

                ColumnLayout {
                    id: strategyColumn

                    width: parent.width
                    spacing: IKActivities.conflictsStrategySpacing

                    Repeater {
                        model: root.strategies

                        delegate: ConflictStrategyCard {
                            id: strategyCard

                            required property var modelData

                            Layout.fillWidth: true
                            title: qsTrId(strategyCard.modelData.titleId)
                            description: strategyCard.modelData.descriptionHasCount
                                         ? qsTrId(strategyCard.modelData.descriptionId).arg(root.countText)
                                         : qsTrId(strategyCard.modelData.descriptionId)
                            iconSource: strategyCard.modelData.iconSource
                            recommended: strategyCard.modelData.recommended
                            selected: root.controller.strategy === strategyCard.modelData.strategy
                            focus: strategyCard.selected
                            onSelectRequested: {
                                root.controller.selectStrategy(strategyCard.modelData.strategy);
                                strategyGroup.forceActiveFocus();
                            }
                        }
                    }
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

            RowLayout {
                Layout.fillWidth: true
                spacing: IKActivities.conflictsButtonSpacing

                IKModalButton {
                    implicitHeight: IKActivities.conflictsButtonHeight
                    text: qsTrId("buttonApplyToXFiles").arg(root.countText)
                    actionEnabled: root.controller.count > 0
                    busy: root.controller.resolutionPending
                    onClicked: root.controller.apply()
                }

                IKModalButton {
                    implicitHeight: IKActivities.conflictsButtonHeight
                    role: IKModalButton.Tonal
                    text: qsTrId("buttonManageIndividually")
                    actionEnabled: !root.controller.resolutionPending
                    onClicked: root.controller.manageIndividually()
                }

                Item {
                    Layout.fillWidth: true
                }
            }
        }

        ScrollBar.vertical: ScrollBar {}
    }
}
