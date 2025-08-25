/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import kDriveCore
import SwiftUI

struct DriverPicker: View {
    let drives: [UIDrive]

    @Binding var selectedDrive: UIDrive

    init(selectedDrive: Binding<UIDrive>, drives: [UIDrive]) {
        _selectedDrive = selectedDrive
        self.drives = drives
    }

    var body: some View {
        Picker("!Sélectionner un drive", selection: $selectedDrive) {
            ForEach(drives) { drive in
                DriveLabel(drive: drive)
                    .drawingGroup(opaque: true, colorMode: .linear)
                    .tag(drive)
            }
        }
        .pickerStyle(.menu)
        .labelsHidden()
        .controlSize(.large)
        .onChange(of: selectedDrive) { newValue in
            switchDrive(newValue)
        }
    }

    private func switchDrive(_ drive: UIDrive) {

    }
}

@available(macOS 14.0, *)
#Preview {
    @Previewable @State var selectedDrive = PreviewHelper.drive1
    DriverPicker(selectedDrive: $selectedDrive, drives: PreviewHelper.drives)
        .scenePadding()
}
