/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

import InfomaniakDI
import kDriveCore
import SwiftUI

struct OnboardingView: View {
    @EnvironmentObject private var rootViewModel: RootViewModel

    var body: some View {
        Text(verbatim: "Onboarding Placeholder")
            .task {
                await connectUserIfPossible()
            }
    }

    private func connectUserIfPossible() async {
        @InjectService var serverBridge: ServerBridgeable
        if let user = await serverBridge.getConnectedUser() {
            rootViewModel.state = .mainView(MainViewModel())
        }
    }
}

#Preview {
    OnboardingView()
}
