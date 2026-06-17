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

import Combine
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import Sentry
import SwiftUI

struct UpdateRequiredView: View {
    @InjectService private var updaterCacheObservable: UpdaterCacheObservable

    @State private var updateState: UIUpdateState = .checking

    @State private var buttonInstallIsDisabled = false
    @State private var failedToStartInstaller = false

    private var updateStatePublisher: UpdateStatePublisher {
        updaterCacheObservable.updateStatePublisher
    }

    var body: some View {
        VStack(spacing: AppPadding.padding32) {
            KDriveResources.foldersStackArrowsCounterclockwise.swiftUIImage
                .resizable()
                .scaledToFit()
                .frame(maxWidth: 160)
                .accessibilityHidden(true)

            VStack(spacing: AppPadding.padding16) {
                Text(KDriveLocalizable.updateRequiredTitle)
                    .font(.Tokens.titleEmphasized)
                Text(KDriveLocalizable.updateRequiredDescription)
                    .font(.Tokens.body)
            }
            .multilineTextAlignment(.center)
            .foregroundStyle(ColorToken.Text.primary.asColor)

            if updateState == .downloading {
                HStack {
                    ProgressView()
                        .progressViewStyle(.circular)

                    Text(KDriveLocalizable.buttonDownloadInProgress)
                        .font(.Tokens.body)
                }
            } else if failedToStartInstaller || updateState == .downloadError || updateState == .updateError {
                Button(KDriveLocalizable.buttonDownloadManually, action: downloadManually)
                    .buttonStyle(.bordered)
            } else {
                Button(KDriveLocalizable.buttonUpdateNow, action: updateApp)
                    .buttonStyle(.borderedProminent)
                    .disabled(buttonInstallIsDisabled)
            }
        }
        .padding(AppPadding.padding16)
        .background(ColorToken.Surface.primary.asColor, in: .rect(cornerRadius: AppRadius.radius16))
        .padding(AppPadding.padding24)
        .task {
            guard let currentUpdateState: KDC.UpdateState = try? await UpdaterJobs().updaterState() else {
                return
            }
            updateState = UIUpdateState(updateState: currentUpdateState)
        }
        .onReceive(updateStatePublisher
            .map(UIUpdateState.init(updateState:))
            .receive(on: RunLoop.main)) { @MainActor updateState in
                self.updateState = updateState
        }
    }

    private func updateApp() {
        Task {
            do {
                buttonInstallIsDisabled = true
                try await UpdaterJobs().startInstaller()
            } catch {
                failedToStartInstaller = true
                IKLogger.data.log("Failed to install required update")
                SentrySDK.capture(error: error)
            }

            buttonInstallIsDisabled = false
        }
    }

    private func downloadManually() {
        let url = URL(string: KDriveLocalizable.downloadAppUrl)!
        NSWorkspace.shared.open(url)
    }
}

#Preview {
    UpdateRequiredView()
}
