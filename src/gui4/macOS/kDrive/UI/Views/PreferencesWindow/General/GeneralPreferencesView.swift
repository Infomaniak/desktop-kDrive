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

import kDriveCoreUI
import SwiftUI

struct GeneralPreferencesView: View {
    var body: some View {
        Form {
            Section {
                HStack {
                    VStack(alignment: .leading) {
                        Text("Mise à jour")
                        Text("kDrive 3.7.6 disponible")
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)

                    Button("Mettre à jour") {
                        // TODO: Update app
                    }
                }

                Toggle("Mises à jour automatiques", isOn: .constant(true))

                IKLabeledContent("Programme bêta") {
                    HStack {
                        Text("Off")
                            .foregroundStyle(.secondary)

                        InformationButton {
                            // TODO: Open Beta modal
                        }
                    }
                }

                IKLabeledContent("À propos de kDrive") {
                    InformationButton {
                        // TODO: Open About kDrive
                    }
                }
            }

            Section {
                Picker("Langue", selection: .constant("fr")) {
                    Text("Français")
                        .tag("fr")
                }

                Picker("Notifications", selection: .constant("off")) {
                    Text("Jamais")
                        .tag("off")
                }

                Toggle("Ouvrir kDrive au démarrage de l'ordinateur", isOn: .constant(true))

                HStack {
                    VStack(alignment: .leading) {
                        Text("Déplacer les fichiers supprimés dans la corbeille de l’ordinateur")
                        Text("Certains éléments ne pourront pas être déplacés.")
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)

                    Toggle("Déplacer les fichiers supprimés dans la corbeille de l’ordinateur", isOn: .constant(true))
                        .labelsHidden()
                }
            }

            Section {
                IKLabeledContent("Besoin d'aide ?") {
                    Button("Support") {
                        // TODO: Open Support
                    }
                }

                IKLabeledContent("Aidez-nous à améliorer kDrive") {
                    Button("Partager une idée") {
                        // TODO: Open Feedback
                    }
                }
            }
        }
        .groupedFormatStyle()
    }
}

#Preview {
    GeneralPreferencesView()
}
