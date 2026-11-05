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
import SwiftUI

struct BlockingErrorView: View {
    @InjectService private var matomo: MatomoUtils
    @InjectService private var vfsConversionCache: VFSConversionCacheObservable

    @StateObject private var reloginViewModel = ReloginViewModel()

    @State private var isShowingGenericError = false
    @State private var isConverting = false
    @State private var userMismatchEmail: String?

    let blockingError: UIBlockingError

    private var buttonIsEnabled: Bool {
        switch blockingError.error {
        case .notRenew:
            if !blockingError.drive.isAdmin {
                return !isConverting
            } else {
                return true
            }
        case .wakingUp, .maintenance, .accessDenied:
            return !isConverting
        case .loggingError:
            return !reloginViewModel.isInProgress
        default:
            return true
        }
    }

    private var title: String {
        guard userMismatchEmail != nil else {
            return blockingError.title
        }
        return KDriveLocalizable.driveLoggingErrorUserMismatchTitle
    }

    private var subtitle: String? {
        guard let userMismatchEmail else {
            return blockingError.subtitle
        }
        return KDriveLocalizable.driveLoggingErrorUserMismatchDescription(userMismatchEmail)
    }

    private var isLoading: Bool {
        blockingError.isLoading || reloginViewModel.isInProgress
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
                Text(title)
                    .font(.Tokens.titleEmphasized)
                if let subtitle {
                    Text(subtitle)
                        .font(.Tokens.body)
                }
            }
            .multilineTextAlignment(.center)
            .foregroundStyle(ColorToken.Text.primary.asColor)
            .padding(.bottom, AppPadding.padding24)

            if isLoading {
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
        .genericErrorAlert(isPresented: $isShowingGenericError)
        .onReceive(vfsConversionCache.isConvertingPublisher(synchroDbId: Int32(blockingError.synchro.dbId))
            .receive(on: RunLoop.main)) {
                isConverting = $0
        }
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
            matomo.track(eventWithCategory: .loginErrorPage, name: "openSignInWeb")
            relogin()
        }
    }

    private func relogin() {
        Task {
            let result = await reloginViewModel.relogin(
                synchroDbId: Int32(blockingError.synchro.dbId),
                anchor: NSApp.keyWindow
            )

            switch result {
            case .success, .cancelled:
                break
            case .userMismatch(let email):
                userMismatchEmail = email
            case .failure:
                isShowingGenericError = true
            }
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
