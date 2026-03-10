/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2025 Infomaniak Network SA

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct SyncedKDriveView: View {
    let drive: UIDrive

    var body: some View {
        Form {
            Section {
                IKLabeledContent("!Emplacement") {
                    Button("/Users/valentin/kDrive", action: openSynchroInFinder)
                        .buttonStyle(.borderless)
                        .tint(.accent)
                }

                IKLabeledContent("!Synchronisation") {
                    Button("!Gérer") {
                        // TODO: Navigate to synchro management
                    }
                    .disabled(true)
                }
            } header: {
                HStack {
                    DriveBadgeView(color: drive.color ?? ColorToken.Drive.defaultColor.asColor)
                    Text(drive.name)
                }
            }

            Section {
                VStack(alignment: .leading) {
                    Text("!Mode de synchronisation des fichiers")
                    Text("!Définissez comment vos fichiers sont stockés et accessibles.")
                        .foregroundStyle(.secondary)
                        .font(.callout)
                }
            }

            Section {
                Button(role: .destructive, action: removePrincipalSynchro) {
                    Text("!Supprimer la synchronisation")
                        .foregroundStyle(.red)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .contentShape(.rect)
                }
                .buttonStyle(.plain)
            }

            Section {
                Button(action: navigateToAdvancedSynchro) {
                    HStack {
                        Text("!Synchronisation avancée")
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .foregroundStyle(.primary)

                        Image(systemName: "chevron.right")
                            .foregroundStyle(.secondary)
                    }
                    .contentShape(.rect)
                }
                .buttonStyle(.plain)
            }
        }
        .groupedFormatStyle()
    }

    private func openSynchroInFinder() {}

    private func removePrincipalSynchro() {}

    private func navigateToAdvancedSynchro() {
        // TODO: Navigate to advanced synchros
    }
}

#Preview {
    SyncedKDriveView(drive: PreviewHelper.drive1)
}
