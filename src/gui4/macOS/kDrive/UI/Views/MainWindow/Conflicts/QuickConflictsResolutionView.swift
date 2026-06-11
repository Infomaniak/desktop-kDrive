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
import kDriveResources
import Sentry
import SwiftUI

enum UIConflictResolutionStrategy: String, Identifiable, CaseIterable {
    var id: String {
        rawValue
    }

    case keepMostRecent
    case keepRemote
    case keepLocal

    static let bestOption = UIConflictResolutionStrategy.keepMostRecent

    var icon: Image {
        switch self {
        case .keepMostRecent:
            return KDriveResources.clock.swiftUIImage
        case .keepRemote:
            return KDriveResources.cloud.swiftUIImage
        case .keepLocal:
            return KDriveResources.computer.swiftUIImage
        }
    }

    var title: String {
        switch self {
        case .keepMostRecent:
            return KDriveLocalizable.labelConflictStrategyKeepMostRecentTitle
        case .keepRemote:
            return KDriveLocalizable.labelConflictStrategyKeepRemoteTitle
        case .keepLocal:
            return KDriveLocalizable.labelConflictStrategyKeepLocalTitle
        }
    }

    func description(count: Int) -> String {
        switch self {
        case .keepMostRecent:
            return KDriveLocalizable.labelConflictStrategyKeepMostRecentDescription
        case .keepRemote:
            return KDriveLocalizable.labelConflictStrategyKeepRemoteDescription(count)
        case .keepLocal:
            return KDriveLocalizable.labelConflictStrategyKeepLocalDescription(count)
        }
    }
}

struct StrategyView: View {
    let icon: Image
    let title: String
    let description: String
    let isBestOption: Bool

    var body: some View {
        GroupBox {
            HStack(alignment: .top, spacing: AppPadding.padding8) {
                BadgeView(image: icon, color: .accentColor)

                VStack(alignment: .leading) {
                    HStack {
                        Text(title)
                            .font(.Tokens.body)
                            .foregroundStyle(.primary)

                        if isBestOption {
                            TagView(text: KDriveLocalizable.labelRecommended)
                        }
                    }
                    Text(description)
                        .font(.Tokens.callout)
                        .foregroundStyle(.secondary)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(AppPadding.padding8)
        }
    }
}

struct QuickConflictsResolutionView: View {
    @State private var isShowingGenericError = false
    @State private var isLoadingButton = false

    @State private var selectedStrategy: UIConflictResolutionStrategy = .keepMostRecent

    let errors: [SynchroError]

    private var errorsCount: Int {
        return errors.count
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: AppPadding.padding24) {
                VStack(alignment: .leading) {
                    Text(KDriveLocalizable.labelManyConflictTitle(errorsCount))
                        .font(.Tokens.body)
                        .foregroundStyle(ColorToken.Text.primary.asColor)
                    Text(KDriveLocalizable.labelChooseConflictVersion)
                        .font(.Tokens.callout)
                        .foregroundStyle(ColorToken.Text.tertiary.asColor)
                }

                Picker(KDriveLocalizable.labelChooseConflictVersion, selection: $selectedStrategy) {
                    ForEach(UIConflictResolutionStrategy.allCases) { strategy in
                        StrategyView(
                            icon: strategy.icon,
                            title: strategy.title,
                            description: strategy.description(count: errorsCount),
                            isBestOption: strategy == .bestOption
                        )
                        .padding(.vertical, AppPadding.padding4)
                        .tag(strategy)
                    }
                }
                .labelsHidden()
                .pickerStyle(.inline)

                HStack {
                    LoadingButton(isLoading: $isLoadingButton, action: applyQuickChange) {
                        Text(KDriveLocalizable.buttonApplyToXFiles(errorsCount))
                    }
                    .buttonStyle(.borderedProminent)

                    Button(KDriveLocalizable.buttonManageIndividually, action: navigateToConflictList)
                }
            }
            .padding(AppPadding.page)
        }
        .scrollBounceBehaviorBasedOnSize()
        .genericErrorAlert(isPresented: $isShowingGenericError)
    }

    private func applyQuickChange() {
        Task {
            isLoadingButton = true

            do {
                let strategy: KDC.ConflictResolutionStrategy
                switch selectedStrategy {
                case .keepMostRecent:
                    strategy = .KeepMostRecent
                case .keepLocal:
                    strategy = .KeepLocal
                case .keepRemote:
                    strategy = .KeepRemote
                }
                let errorDbIds = errors.map { Int32($0.metadata.dbId) }

                try await ErrorJobs().resolveConflictsQuick(errorDbIds: errorDbIds, strategy: strategy)

                @InjectService var router: MainViewRouter
                router.removeLast()
            } catch {
                isShowingGenericError = true
                SentrySDK.capture(error: error)
            }

            isLoadingButton = false
        }
    }

    private func navigateToConflictList() {
        @InjectService var router: MainViewRouter
        router.append(.conflictsList)
    }
}

#Preview {
    QuickConflictsResolutionView(errors: Array(repeating: PreviewHelper.synchroError, count: 4))
}
