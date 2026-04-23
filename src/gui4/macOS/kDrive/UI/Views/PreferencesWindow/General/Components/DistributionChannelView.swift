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

struct DistributionChannelView: View {
    @Environment(\.dismiss) private var dismiss

    @ObservedObject var repository: PreferencesRepository

    @State private var betaOption = BetaOption.doNotJoin

    var body: some View {
        VStack(alignment: .leading) {
            Text(KDriveLocalizable.betaProgramTitle)
                .font(.Tokens.headline)
                .foregroundStyle(ColorToken.Text.primary.asColor)
                .padding(.bottom, AppPadding.padding4)

            Text(KDriveLocalizable.betaProgramDescription)
                .font(.Tokens.body)
                .foregroundStyle(ColorToken.Text.tertiary.asColor)

            OptionPicker(KDriveLocalizable.accessibilityBetaProgramPicker, options: BetaOption.allCases, selection: $betaOption)
                .labelsHidden()
        }
        .onAppear {
            betaOption = BetaOption(repository.parametersInfo.distributionChannel)
        }
        .padding()
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                Button(KDriveLocalizable.buttonValidate) {
                    updateDistributionChannel()
                    dismiss()
                }
            }
            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonCancel, role: .cancel) {
                    dismiss()
                }
            }
        }
    }

    private func updateDistributionChannel() {
        Task {
            let currentValue = repository.parametersInfo.distributionChannel

            var newValue: UIDistributionChannel
            switch betaOption {
            case .doNotJoin:
                guard currentValue != .prod && currentValue != .next && currentValue != .legacy else { return }
                newValue = .prod
            case .beta:
                guard currentValue != .beta else { return }
                newValue = .beta
            case .internal:
                guard currentValue != .internal else { return }
                newValue = .internal
            case .test:
                guard currentValue != .test else { return }
                newValue = .test
            }

            try? await repository.update(\.distributionChannel, value: newValue)
        }
    }
}

#Preview {
    DistributionChannelView(repository: PreferencesRepository())
}
