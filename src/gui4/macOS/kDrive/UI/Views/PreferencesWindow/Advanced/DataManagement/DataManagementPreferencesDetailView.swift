/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

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

struct DataManagementPreferencesDetailView: View {
    @State private var allowTracking = false

    let item: DataManagementItem
    let repository: PreferencesRepository

    var body: some View {
        Form {
            VStack(alignment: .leading, spacing: AppPadding.padding8) {
                item.image
                    .resizable()
                    .scaledToFit()
                    .frame(height: 24)
                    .padding(.vertical, AppPadding.padding4)

                Text(item.description)
                    .font(.Tokens.callout)
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }

            Toggle(KDriveLocalizable.labelAllowTracking, isOn: $allowTracking)
        }
        .groupedFormatStyle()
        .onAppear {
            allowTracking = repository.parametersInfo[keyPath: item.keyPath]
        }
        .onChange(of: allowTracking) { newValue in
            updateValue(\.$allowTracking, item.keyPath, newValue: newValue)
        }
    }

    private func updateValue<T: Equatable>(
        _ stateKeyPath: KeyPath<Self, Binding<T>>,
        _ repositoryKeyPath: WritableKeyPath<UIParametersInfo, T>,
        newValue: T
    ) {
        Task {
            guard newValue != repository.parametersInfo[keyPath: repositoryKeyPath] else { return }

            do {
                try await repository.update(repositoryKeyPath, value: newValue)
            } catch {
                self[keyPath: stateKeyPath].wrappedValue = repository.parametersInfo[keyPath: repositoryKeyPath]
            }
        }
    }
}

#Preview {
    DataManagementPreferencesDetailView(item: .matomo, repository: PreferencesRepository())
}
