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
import kDrive.UI

Item {
    id: root

    required property var controller
    // Main sidebar controller: names the selected synchronization the way the synchronization selector does.
    required property var selection

    readonly property bool diskUnavailable: controller.state === StorageController.Unavailable
    readonly property bool diskLoading: controller.state === StorageController.Loading

    Component.onCompleted: controller.setViewActive(true)
    Component.onDestruction: controller.setViewActive(false)

    ScrollView {
        id: scrollView

        anchors.fill: parent
        contentWidth: availableWidth
        leftPadding: IKSpacing.page
        rightPadding: IKSpacing.page
        topPadding: IKSpacing.s32
        bottomPadding: IKSpacing.s24

        Column {
            width: scrollView.availableWidth
            spacing: IKStorage.cardSpacing

            StorageUsageCard {
                width: parent.width
                visible: !root.diskUnavailable
                title: root.controller.usesDefaultVolumeName ? qsTrId("storageThisComputer") : root.controller.volumeName
                usageText: root.controller.diskAlmostFull
                           ? qsTrId("labelStorageUsed-plural").arg(root.controller.usedSizeText)
                           : qsTrId("storageUsageLabel").arg(root.controller.usedSizeText).arg(root.controller.totalSizeText)
                loading: root.diskLoading
                animated: root.controller.animatedUpdate
                availableRatio: root.controller.availableRatio
                segments: [
                    StorageSegment {
                        ratio: root.controller.syncRatio
                        color: IKColors.storageBarSync
                        measured: root.controller.syncMeasured
                    },
                    StorageSegment {
                        ratio: root.controller.otherSyncsRatio
                        color: IKColors.storageBarOtherSyncs
                        measured: root.controller.otherSyncsMeasured
                    },
                    // The remainder of the used space: measured kDrive segments take their share from it.
                    StorageSegment {
                        ratio: root.controller.otherRatio
                        color: IKColors.storageBarOther
                    }
                ]
                entries: [
                    StorageLegendEntry {
                        dotColor: IKColors.storageLegendSync
                        label: qsTrId("storageComputerUsedByKDrive")
                        value: root.controller.syncSizeText
                        valueLoading: !root.controller.syncMeasured
                        chipTitle: root.selection.currentTitle
                        chipColor: root.selection.currentDriveColor
                        chipAdvancedSync: root.selection.currentEntryType === SyncSelectorModel.AdvancedSync
                    },
                    StorageLegendEntry {
                        dotColor: IKColors.storageLegendOtherSyncs
                        label: qsTrId("storageComputerUsedByOtherSyncs")
                        value: root.controller.otherSyncsSizeText
                        valueLoading: !root.controller.otherSyncsMeasured
                        shown: !root.controller.otherSyncsEmpty
                    },
                    StorageLegendEntry {
                        dotColor: IKColors.storageLegendOther
                        label: qsTrId("storageComputerUsedByOther")
                        value: root.controller.otherSizeText
                        valueLoading: !root.controller.syncMeasured || !root.controller.otherSyncsMeasured
                    },
                    StorageLegendEntry {
                        dotColor: IKColors.storageFree
                        label: qsTrId("storageComputerFreeSpace")
                        value: root.controller.availableSizeText
                        valueLoading: root.diskLoading
                    }
                ]
            }

            Rectangle {
                width: parent.width
                height: errorState.implicitHeight
                visible: root.diskUnavailable
                color: IKColors.storageSurface
                radius: IKRadius.r12

                StorageErrorState {
                    id: errorState

                    anchors.fill: parent
                    controller: root.controller
                }
            }

            StorageActionCard {
                width: parent.width
                visible: root.controller.diskAlmostFull && !root.diskUnavailable
                iconSource: "qrc:/assets/main/storage/computer.svg"
                title: qsTrId("errNotEnoughDiskSpaceTitle")
                description: qsTrId("storageDiskAlmostFullDescription")
                actionText: qsTrId("buttonManageDiskSpace")

                onActionTriggered: root.controller.openDiskManagement()
            }

            StorageUsageCard {
                // The upgrade link replaces the usage once the subscription is full.
                readonly property bool showsUpgradeLink: root.controller.quotaFull && root.controller.canUpgradeSubscription

                width: parent.width
                visible: root.controller.quotaAvailable
                title: root.controller.subscriptionName.length > 0
                       ? qsTrId("storageSubscriptionTitle").arg(root.controller.subscriptionName)
                       : qsTrId("labelMySubscription")
                usageText: showsUpgradeLink
                           ? ""
                           : qsTrId("storageUsageLabel").arg(root.controller.quotaUsedSizeText)
                                                        .arg(root.controller.quotaTotalSizeText)
                availableRatio: 1 - root.controller.quotaUsedRatio
                headerAccessory: root.controller.canUpgradeSubscription ? upgradeAccessory : null
                segments: [
                    StorageSegment {
                        ratio: root.controller.quotaUsedRatio
                        color: IKColors.storageBarSync
                    }
                ]
                entries: [
                    StorageLegendEntry {
                        dotColor: IKColors.storageLegendSync
                        label: qsTrId("storageOnlineUsed")
                        value: root.controller.quotaUsedSizeText
                        chipTitle: root.controller.driveName
                        chipColor: root.selection.currentDriveColor
                    },
                    StorageLegendEntry {
                        dotColor: IKColors.storageFree
                        label: qsTrId("storageOnlineFree")
                        value: root.controller.quotaFreeSizeText
                    }
                ]
            }

            StorageSyncCard {
                width: parent.width

                onManageRequested: root.controller.manageSynchronization()
            }
        }
    }

    Component {
        id: upgradeAccessory

        Row {
            spacing: IKStorage.headerAccessorySpacing

            IKLinkButton {
                anchors.verticalCenter: parent.verticalCenter
                external: true
                text: qsTrId("buttonUpgradeSubscription")
                visible: root.controller.quotaFull

                onClicked: root.controller.openSubscriptionUpgrade()
            }

            AbstractButton {
                id: cartButton

                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: IKStorage.cartIconSize
                implicitHeight: IKStorage.cartIconSize
                // Next to the upgrade link, the cart is decorative: the link alone takes focus and is announced.
                focusPolicy: root.controller.quotaFull ? Qt.NoFocus : Qt.StrongFocus
                hoverEnabled: true
                Accessible.ignored: root.controller.quotaFull
                Accessible.role: Accessible.Button
                Accessible.name: qsTrId("buttonUpgradeSubscription")

                // The control owns its content item geometry: the icon keeps its own size inside a wrapper.
                contentItem: Item {
                    IKTintedIcon {
                        anchors.centerIn: parent
                        width: IKStorage.cartIconSize
                        height: IKStorage.cartIconSize
                        source: "qrc:/assets/main/storage/cart-add.svg"
                        color: IKColors.storageCartIcon
                    }
                }

                onClicked: root.controller.openSubscriptionUpgrade()

                // Drawn around the glyph without enlarging the control, so the header keeps its alignment.
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width + 2 * IKStorage.cartFocusRingPadding
                    height: parent.height + 2 * IKStorage.cartFocusRingPadding
                    radius: IKRadius.r4
                    color: "transparent"
                    border.width: IKLinkTokens.focusRingWidth
                    border.color: IKColors.accentPrimary
                    visible: cartButton.visualFocus
                }

                // Icon-only control: the tooltip states that the action opens the web browser.
                IKToolTip {
                    targetButton: cartButton
                    text: qsTrId("buttonUpgradeSubscription")
                }
            }
        }
    }
}
