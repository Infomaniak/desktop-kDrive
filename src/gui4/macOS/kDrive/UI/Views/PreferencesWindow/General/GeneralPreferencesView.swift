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
import kDriveResources
import SwiftUI

struct GeneralPreferencesView: View {
    var body: some View {
        Form {
            Section {
                HStack {
                    VStack(alignment: .leading) {
                        Text(KDriveLocalizable.updateSettings)
                        Text(KDriveLocalizable.updateAvailable("3.7.6"))
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)

                    Button(KDriveLocalizable.buttonUpdate) {
                        // TODO: Update app
                    }
                }

                Toggle(KDriveLocalizable.automaticUpdatesSetting, isOn: .constant(true))

                IKLabeledContent(KDriveLocalizable.betaSettings) {
                    HStack {
                        Text("Off")
                            .foregroundStyle(.secondary)

                        InformationButton {
                            // TODO: Open Beta modal
                        }
                    }
                }

                IKLabeledContent(KDriveLocalizable.aboutKDrive) {
                    InformationButton {
                        // TODO: Open About kDrive
                    }
                }
            }

            Section {
                Picker(KDriveLocalizable.languageSetting, selection: .constant("fr")) {
                    Text("Français")
                        .tag("fr")
                }

                Picker(KDriveLocalizable.labelNotifications, selection: .constant("off")) {
                    Text(KDriveLocalizable.notificationsDisabledNever)
                        .tag("off")
                }

                Toggle(KDriveLocalizable.openKDriveAtStartupSetting, isOn: .constant(true))

                HStack {
                    VStack(alignment: .leading) {
                        Text(KDriveLocalizable.moveDeletedFilesToRecycleBinSetting)
                        Text(KDriveLocalizable.moveDeletedFilesToRecycleBinWarning)
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)

                    Toggle(KDriveLocalizable.moveDeletedFilesToRecycleBinSetting, isOn: .constant(true))
                        .labelsHidden()
                }
            }

            Section {
                IKLabeledContent(KDriveLocalizable.needHelpSetting) {
                    Button(KDriveLocalizable.buttonHelpdesk) {
                        // TODO: Open Support
                    }
                }

                IKLabeledContent(KDriveLocalizable.feedbackSetting) {
                    Button(KDriveLocalizable.buttonFeedback) {
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
