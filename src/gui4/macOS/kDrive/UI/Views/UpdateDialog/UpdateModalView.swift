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

import kDriveCore
import kDriveCoreUI
import kDriveResources
import OrderedCollections
import Sentry
import SwiftUI

@MainActor
struct UpdateModalView: View {
    private static let windowWidth: CGFloat = 460
    private static let releaseNotesHeight: CGFloat = 140

    let versionInfo: VersionInfo
    let onDismiss: () -> Void

    @State private var releaseNotes: String?
    @State private var isShowingReleaseNotes = false

    private var currentVersion: String {
        Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? ""
    }

    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding16) {
            HStack(spacing: AppPadding.padding8) {
                KDriveResources.kdriveAppIcon.swiftUIImage
                    .resizable()
                    .frame(width: 24, height: 24)
                Text(Constants.appName)
                    .font(.Tokens.bodyEmphasized)
                    .foregroundStyle(ColorToken.Text.primary.asColor)
            }

            VStack(alignment: .leading, spacing: AppPadding.padding4) {
                Text(KDriveLocalizable.updateDialogTitle)
                    .font(.Tokens.title2Emphasized)
                    .foregroundStyle(ColorToken.Text.primary.asColor)
                Text(KDriveLocalizable.updateDialogSubtitle(versionInfo.tag, currentVersion))
                    .font(.Tokens.subheadline)
                    .foregroundStyle(ColorToken.Text.secondary.asColor)
            }

            Text(KDriveLocalizable.updateDialogDescription)
                .font(.Tokens.body)
                .foregroundStyle(ColorToken.Text.primary.asColor)
                .fixedSize(horizontal: false, vertical: true)

            if let releaseNotes {
                VStack(spacing: 0) {
                    Button {
                        withAnimation { isShowingReleaseNotes.toggle() }
                    } label: {
                        HStack {
                            Text(KDriveLocalizable.updateDialogSeeWhatsNew)
                                .font(.Tokens.body)
                                .foregroundStyle(ColorToken.Text.primary.asColor)
                                .frame(maxWidth: .infinity, alignment: .leading)

                            Image(systemName: "chevron.down")
                                .font(.Tokens.subheadline)
                                .foregroundStyle(ColorToken.Text.secondary.asColor)
                                .rotationEffect(.degrees(isShowingReleaseNotes ? 180 : 0))
                        }
                        .padding(AppPadding.padding12)
                        .contentShape(.rect)
                    }
                    .buttonStyle(.plain)

                    if isShowingReleaseNotes {
                        ReleaseNotesWebView(releaseNotes: releaseNotes)
                            .frame(height: Self.releaseNotesHeight)
                            .padding([.horizontal, .bottom], AppPadding.padding12)
                    }
                }
                .background(ColorToken.Surface.secondary.asColor, in: .rect(cornerRadius: AppRadius.radius8))
            }

            HStack(spacing: AppPadding.padding8) {
                Button(KDriveLocalizable.updateDialogIgnoreVersion, action: ignoreVersion)
                    .buttonStyle(.plain)
                    .font(.Tokens.subheadline)
                    .foregroundStyle(ColorToken.Text.secondary.asColor)
                    .frame(maxWidth: .infinity)

                Button(KDriveLocalizable.updateDialogRemindLater, action: onDismiss)
                    .frame(maxWidth: .infinity)

                Button(KDriveLocalizable.updateDialogInstallNow, action: installNow)
                    .buttonStyle(.borderedProminent)
                    .frame(maxWidth: .infinity)
            }
            .controlSize(.large)
        }
        .padding([.horizontal, .bottom], AppPadding.page)
        .frame(width: Self.windowWidth)
        .task {
            await loadReleaseNotes()
        }
    }

    private func loadReleaseNotes() async {
        let languageCode = String(Locale.current.languageCode ?? "en")
        let candidateURLs = OrderedSet([
            URLConstants.releaseNote(versionTag: versionInfo.tag, languageCode: languageCode),
            URLConstants.releaseNote(versionTag: versionInfo.tag, languageCode: "en")
        ])

        for url in candidateURLs {
            guard let (data, response) = try? await URLSession.shared.data(from: url),
                  let httpResponse = response as? HTTPURLResponse,
                  (200 ... 299).contains(httpResponse.statusCode) else {
                continue
            }

            let html = String(decoding: data, as: UTF8.self)
            guard !html.isEmpty else { continue }

            releaseNotes = html
            return
        }
    }

    private func installNow() {
        Task {
            do {
                try await UpdaterJobs().startInstaller()
            } catch {
                SentrySDK.capture(error: error)
                IKLogger.general.error("[KD] Failed to start installer: \(error)")
            }
            onDismiss()
        }
    }

    private func ignoreVersion() {
        Task {
            do {
                try await UpdaterJobs().skipVersion(version: versionInfo.tag)
            } catch {
                SentrySDK.capture(error: error)
                IKLogger.general.error("[KD] Failed to skip version: \(error)")
            }
            onDismiss()
        }
    }
}

#Preview {
    UpdateModalView(versionInfo: PreviewHelper.versionInfo) {}
}
