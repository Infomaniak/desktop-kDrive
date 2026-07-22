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
import kDriveResources
import SwiftUI

public struct SynchroModePicker: View {
    @LazyInjectService private var matomo: MatomoUtils
    @State private var isConvertingSynchro = false

    @State private var selectedMode: UISynchroMode
    @State private var modePendingConfirmation: UISynchroMode?

    private var isCallFromAdvancedSync: Bool

    @Binding var synchroMode: UISynchroMode

    let synchroDbId: Int

    public init(synchroDbId: Int, synchroMode: Binding<UISynchroMode>, isCallFromAdvancedSync: Bool = false) {
        self.synchroDbId = synchroDbId
        _synchroMode = synchroMode
        _selectedMode = State(initialValue: synchroMode.wrappedValue)
        self.isCallFromAdvancedSync = isCallFromAdvancedSync
    }

    public var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding16) {
            VStack(alignment: .leading, spacing: AppPadding.padding2) {
                Text(KDriveLocalizable.fileSyncMode)
                Text(KDriveLocalizable.fileSyncModeDescription)
                    .foregroundStyle(.secondary)
                    .font(.callout)
            }

            Picker(KDriveLocalizable.accessibilitySelectSynchroMode, selection: $selectedMode) {
                ForEach(UISynchroMode.allCases) { mode in
                    UISynchroModeCell(mode: mode)
                        .tag(mode)
                }
            }
            .pickerStyle(.radioGroup)
            .labelsHidden()
        }
        .disabled(isConvertingSynchro)
        .observingSynchroConversion(synchroDbId: synchroDbId, isConverting: $isConvertingSynchro)
        .onChange(of: selectedMode) { newValue in
            guard newValue != synchroMode else { return }
            modePendingConfirmation = newValue
        }
        .onChange(of: synchroMode) { newValue in
            guard newValue != selectedMode else { return }
            selectedMode = newValue
        }
        .alert(
            KDriveLocalizable.dialogSyncModeChangeWarningTitle,
            isPresented: Binding(
                get: { modePendingConfirmation != nil },
                set: { isPresented in
                    if !isPresented {
                        modePendingConfirmation = nil
                    }
                }
            ),
            presenting: modePendingConfirmation
        ) { pendingMode in
            Button(KDriveLocalizable.buttonCancel, role: .cancel) {
                let (category, name) = matomoByParent(isCancelChoice: true)
                matomo.track(eventWithCategory: category, name: name)
                selectedMode = synchroMode
            }
            Button(pendingMode == .storeOnline
                ? KDriveLocalizable.buttonChangeToOnline
                : KDriveLocalizable.buttonChangeToOffline) {
                    let (category, name) = matomoByParent(isCancelChoice: false)
                    matomo.track(eventWithCategory: category, name: name, value: pendingMode != .storeOnline)
                    synchroMode = pendingMode
                }
        } message: { _ in
            Text(KDriveLocalizable.dialogSyncModeChangeWarningContent)
        }
    }

    func matomoByParent(isCancelChoice: Bool) -> (category: MatomoUtils.EventCategory, name: String) {
        switch isCallFromAdvancedSync {
        case true: return (.advancedSettingsPage, isCancelChoice ? "cancelSyncModeSwitch" : "confirmSyncModeSwitch")
        case false: return (.driveManagementPage, isCancelChoice ? "cancelSyncModeSwitch" : "confirmSyncModeSwitch")
        }
    }
}

#Preview {
    SynchroModePicker(synchroDbId: 0, synchroMode: .constant(.storeOnline))
}
