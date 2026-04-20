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

import QtQuick
import QtQuick.Controls
import kDrive.UI

Window {
    id: mainWindow
    visible: true
    title: "kDrive"

    // Transitional bootstrap view used while the real screens are being built.
    Rectangle {
        anchors.fill: parent
        color: IKColors.accentPrimary

        Column {
            anchors {
                fill: parent
                margins: 16
            }
            spacing: 12

            Text {
                text: "Bootstrap C2 models"
                color: IKColors.textPrimary
            }

            Row {
                spacing: 16
                width: parent.width
                height: parent.height - 120

                Rectangle {
                    width: (parent.width - 16) / 2
                    height: parent.height
                    color: "#22000000"
                    radius: 8

                    Column {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        Text {
                            text: "Drives (" + driveListView.count + ")"
                            color: IKColors.textPrimary
                        }

                        ListView {
                            id: driveListView
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.topMargin: 28
                            model: driveListModel
                            clip: true
                            delegate: Text {
                                width: parent ? parent.width : 200
                                color: IKColors.textPrimary
                                text: name + (selected ? " (active)" : "")
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                Rectangle {
                    width: (parent.width - 16) / 2
                    height: parent.height
                    color: "#22000000"
                    radius: 8

                    Column {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        Text {
                            text: "Syncs (" + syncListView.count + ")"
                            color: IKColors.textPrimary
                        }

                        ListView {
                            id: syncListView
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.topMargin: 28
                            model: syncListModel
                            clip: true
                            delegate: Text {
                                width: parent ? parent.width : 200
                                color: IKColors.textPrimary
                                text: localPath + (selected ? " (active)" : "")
                                elide: Text.ElideMiddle
                            }
                        }
                    }
                }
            }

            Text {
                text: ThemeMode._mode === ThemeMode.System ? "System"
                    : ThemeMode._mode === ThemeMode.Light  ? "Light"
                    : "Dark"
                color: IKColors.textPrimary
            }

            Button {
                text: "Toggle theme"
                onClicked: {
                    if (ThemeMode._mode === ThemeMode.System)
                        ThemeMode.set(ThemeMode.Dark);
                    else if (ThemeMode._mode === ThemeMode.Dark)
                        ThemeMode.set(ThemeMode.Light);
                    else
                        ThemeMode.set(ThemeMode.System);
                }
            }
        }
    }
}
