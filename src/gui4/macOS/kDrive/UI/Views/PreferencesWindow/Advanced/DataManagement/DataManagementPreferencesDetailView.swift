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

import Combine
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct DataManagementPreferencesDetailView: View {
    let item: DataManagementItem
    let repository: PreferencesRepository

    @State private var allowTracking = false

    var body: some View {
        Form {
            VStack(alignment: .leading, spacing: AppPadding.padding8) {
                item.image
                    .padding(.vertical, AppPadding.padding4)
                    .frame(width: 104, height: 32, alignment: .leading)
                Text(item.description)
                    .font(.Tokens.body)
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }
            Toggle(KDriveLocalizable.labelAllowTracking, isOn: $allowTracking)
        }
        .groupedFormatStyle()
        .onChange(of: allowTracking, perform: { newValue in
            switch item {
            case .matomo:
                updateValue(\.$allowTracking, \.isMatomoEnabled, newValue: newValue)
            case .sentry:
                updateValue(\.$allowTracking, \.isSentryEnabled, newValue: newValue)
            }

        })
        .onAppear {
            allowTracking = getTracking()
        }
    }

    func getTracking() -> Bool {
        var value = false
        switch item {
        case .matomo:
            value = repository.parametersInfo.isMatomoEnabled
        case .sentry:
            value = repository.parametersInfo.isSentryEnabled
        }
        return value
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
