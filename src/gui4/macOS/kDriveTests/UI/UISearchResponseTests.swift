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

import Foundation
@testable import kDriveCoreUI
import Testing

struct UISearchResponseTests {
    private func makeSearchResponse(isAvailableLocally: Bool, isHydrated: Bool, type: UINodeType = .file) -> UISearchResponse {
        UISearchResponse(
            id: "1",
            name: "file.txt",
            type: type,
            path: "/test/file.txt",
            modifiedDate: Date(),
            size: 1024,
            isAvailableLocally: isAvailableLocally,
            isHydrated: isHydrated
        )
    }

    @Test(
        "Files open locally only when available locally and either hydrated or synchro not paused",
        arguments: [
            (isAvailableLocally: true, isHydrated: true, isSynchroPaused: false, expected: true),
            (isAvailableLocally: true, isHydrated: true, isSynchroPaused: true, expected: true),
            (isAvailableLocally: true, isHydrated: false, isSynchroPaused: false, expected: true),
            (isAvailableLocally: true, isHydrated: false, isSynchroPaused: true, expected: false),
            (isAvailableLocally: false, isHydrated: false, isSynchroPaused: false, expected: false),
            (isAvailableLocally: false, isHydrated: false, isSynchroPaused: true, expected: false)
        ]
    )
    func opensLocally(testCase: (isAvailableLocally: Bool, isHydrated: Bool, isSynchroPaused: Bool, expected: Bool)) {
        let file = makeSearchResponse(isAvailableLocally: testCase.isAvailableLocally, isHydrated: testCase.isHydrated)
        #expect(file.opensLocally(isSynchroPaused: testCase.isSynchroPaused) == testCase.expected)
    }

    @Test("Available non-hydrated directories open locally while synchro is paused")
    func nonHydratedDirectoryOpensLocallyWhilePaused() {
        let directory = makeSearchResponse(isAvailableLocally: true, isHydrated: false, type: .directory)
        #expect(directory.opensLocally(isSynchroPaused: true))
    }

    @Test("Paused synchro statuses", arguments: [
        (status: UISynchroStatus.starting, expected: false),
        (status: UISynchroStatus.running, expected: false),
        (status: UISynchroStatus.idle, expected: false),
        (status: UISynchroStatus.pauseAsked, expected: true),
        (status: UISynchroStatus.paused, expected: true),
        (status: UISynchroStatus.stopAsked, expected: true),
        (status: UISynchroStatus.stopped, expected: true),
        (status: UISynchroStatus.error, expected: false)
    ])
    func synchroStatusIsPaused(testCase: (status: UISynchroStatus, expected: Bool)) {
        #expect(testCase.status.isPaused == testCase.expected)
    }
}
