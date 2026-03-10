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
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct VersionManagementView: View {
    @LazyInjectService private var updaterCacheObservable: UpdaterCacheObservable

    @State private var updateState = UIUpdateState.upToDate
    @State private var newVersionInfo: UIVersionInfo?

    @State private var isShowingError = false
    @State private var error: DomainError?

    @State private var versionTask: Task<Void, Never>?

    @ObservedObject var repository: PreferencesRepository

    enum DomainError: LocalizedError {
        case cannotStartInstall

        var errorDescription: String? {
            switch self {
            case .cannotStartInstall:
                return KDriveLocalizable.errorStartingInstaller
            }
        }
    var body: some View {
        HStack {
            if updateState.isUpdateAvailable, let newVersionInfo {
                VStack(alignment: .leading) {
                    Text(KDriveLocalizable.updateSettings)
                    Text(KDriveLocalizable.updateAvailable(newVersionInfo.tag))
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                Button(KDriveLocalizable.buttonUpdate, action: updateApp)
            } else {
                VStack(alignment: .leading) {
                    Text(KDriveLocalizable.updateSettings)
                    Text(KDriveLocalizable.appUpToDate)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
        }
        .task {
            guard let currentUpdateState = try? await UpdaterJobs().updaterState() else {
                return
            }

            let updateState = UIUpdateState(updateState: currentUpdateState)
            handleUpdateState(updateState)
        }
        .onReceive(updaterCacheObservable.updateStatePublisher.map { UIUpdateState(updateState: $0) }) {
            handleUpdateState($0)
        }
        .alert(isPresented: $isShowingError, error: error) {}
    }

    private func updateApp() {
        Task {
            do {
                try await UpdaterJobs().startInstaller()
            } catch {
                self.error = .cannotStartInstall
                isShowingError = true
            }
        }
    }

    private func handleUpdateState(_ state: UIUpdateState) {
        updateState = state

        versionTask?.cancel()
        versionTask = Task { @MainActor in
            let channel = repository.parametersInfo.distributionChannel.toKDCVersionChannel()
            guard let newVersion = try? await UpdaterJobs().versionInfo(channel: channel) else {
                return
            }

            guard !Task.isCancelled else { return }
            withAnimation {
                newVersionInfo = UIVersionInfo(versionInfo: newVersion)
            }
        }
    }
}

#Preview {
    VersionManagementView(repository: PreferencesRepository())
}
