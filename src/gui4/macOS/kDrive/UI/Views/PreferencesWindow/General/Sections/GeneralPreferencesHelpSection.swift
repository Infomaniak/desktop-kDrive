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

struct GeneralPreferencesHelpSection: View {
    enum URLConstants {
        static let help = URL(string: "https://www.infomaniak.com/gtl/help")!
        static let feedback = URL(string: "https://feedback.userreport.com/652ad8f0-84c8-4a21-9e31-7a8bd7134f46#ideas/popular")!
    }

    @Environment(\.openURL) private var openURL

    var body: some View {
        Section {
            IKLabeledContent(KDriveLocalizable.needHelpSetting) {
                Button(KDriveLocalizable.buttonHelpdesk) {
                    openURL(URLConstants.help)
                }
            }

            IKLabeledContent(KDriveLocalizable.feedbackSetting) {
                Button(KDriveLocalizable.buttonFeedback) {
                    openURL(URLConstants.feedback)
                }
            }
        }
    }
}

#Preview {
    GeneralPreferencesHelpSection()
}
