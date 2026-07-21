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

import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import SwiftUI

struct BlockingErrorView: View {
    @InjectService private var matomo: MatomoUtils

    @State private var isConvertingSynchro = false
    @State private var isShowingGenericError = false

    let blockingError: UIBlockingError

    private var buttonIsEnabled: Bool {
        switch blockingError.error {
        case .notRenew:
            if !blockingError.drive.isAdmin {
                return !isConvertingSynchro
            } else {
                return true
            }
        case .wakingUp, .maintenance, .accessDenied:
            return !isConvertingSynchro
        default:
            return true
        }
    }

    var body: some View {
        VStack(spacing: 0) {
            DriveLabel(
                drive: blockingError.drive,
                badgeIcon: blockingError.badgeIcon,
                badgeBackgroundColor: blockingError.badgeBackgroundColor,
                badgeColor: blockingError.badgeColor
            )
            .padding(.bottom, AppPadding.padding64)

            VStack(spacing: AppPadding.padding16) {
                Text(blockingError.title)
                    .font(.Tokens.titleEmphasized)
                if let subtitle = blockingError.subtitle {
                    Text(subtitle)
                        .font(.Tokens.body)
                }
            }
            .multilineTextAlignment(.center)
            .foregroundStyle(ColorToken.Text.primary.asColor)
            .padding(.bottom, AppPadding.padding24)

            if blockingError.isLoading {
                ProgressView()
                    .controlSize(.small)
                    .padding(.bottom, AppPadding.padding24)
            }

            if let actionTitle = blockingError.actionTitle {
                Button(actionTitle, action: handleAction)
                    .buttonStyle(.borderedProminent)
                    .disabled(!buttonIsEnabled)
            }
        }
        .padding(AppPadding.padding32)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(ColorToken.Surface.primary.asColor, in: .rect(cornerRadius: AppRadius.radius16))
        .padding(AppPadding.padding24)
        .observingSynchroConversion(synchroDbId: blockingError.synchro.dbId, isConverting: $isConvertingSynchro)
        .genericErrorAlert(isPresented: $isShowingGenericError)
    }

    private func handleAction() {
        switch blockingError.error {
        case .asleep:
            matomo.track(eventWithCategory: .asleepErrorPage, name: "openRenewWeb")
            NSWorkspace.shared.open(URLConstants.kDrive(for: blockingError.drive.driveId))
        case .notRenew:
            if blockingError.drive.isAdmin {
                matomo.track(eventWithCategory: .notRenewErrorPage, name: "openRenewWeb")
                @InjectService var nodeURLGenerator: NodeURLGenerator
                let shopURL = nodeURLGenerator.shopURL(forDriveId: Int(blockingError.drive.driveId))
                NSWorkspace.shared.open(shopURL)
            } else {
                matomo.track(eventWithCategory: .notRenewErrorPage, name: "startSync")
                restartSynchro()
            }
        case .wakingUp:
            matomo.track(eventWithCategory: .asleepErrorPage, name: "startSync")
            restartSynchro()
        case .maintenance:
            matomo.track(eventWithCategory: .maintenanceErrorPage, name: "startSync")
            restartSynchro()
        case .accessDenied:
            matomo.track(eventWithCategory: .driveAccessDeniedPage, name: "startSync")
            restartSynchro()
        case .loggingError:
            matomo.track(eventWithCategory: .logginErrorPage, name: "openSignInWeb")
            @InjectService var router: MainWindowRouter
            router.navigate(to: .onboarding(nil, nil, .login))
        }
    }

    private func restartSynchro() {
        Task {
            do {
                try await SyncJobs().startSync(syncDbId: Int32(blockingError.synchro.dbId))
            } catch {
                isShowingGenericError = true
            }
        }
    }
}

#Preview {
    VStack {
        ForEach(BlockingSynchroError.allCases, id: \.self) { error in
            BlockingErrorView(blockingError: PreviewHelper.blockingErrorFor(syncError: error))
                .frame(minWidth: 512)
                .fixedSize(horizontal: false, vertical: true)
        }
    }
}
