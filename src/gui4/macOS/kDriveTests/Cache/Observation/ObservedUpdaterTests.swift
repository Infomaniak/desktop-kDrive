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
import Foundation
@testable import kDriveCore
import Testing

extension ObservedUpdater {
    var receivedValues: AsyncStream<KDC.UpdateState> {
        AsyncStream { continuation in
            let cancellable = $wrappedValue
                .sink { value in
                    if let value = value {
                        continuation.yield(value)
                    }
                }

            continuation.onTermination = { _ in cancellable.cancel() }
        }
    }
}

@MainActor
struct ObservedUpdaterTests {
    @Test(.timeLimit(.minutes(1)))
    func setObservedUpdateState() async {
        // GIVEN
        let cache = UpdaterStateCache()
        let initialState = await cache.getUpdateState()
        #expect(initialState == nil, "Cache should initially be empty")

        @ObservedUpdater(updaterObservable: cache) var observedState: KDC.UpdateState?
        let receivedValues = $observedState.receivedValues // Start to save the received values

        #expect(observedState == nil, "UpdateState should initially be nil")

        // WHEN
        let expectedState = KDC.UpdateState.Checking
        await cache.setUpdateState(expectedState)

        // THEN
        _ = await receivedValues.first(where: { _ in true })

        let cachedState = await cache.getUpdateState()
        #expect(cachedState == expectedState, "The cache should have been updated with the state")
        #expect(observedState == expectedState, "The observed state should match the one we just set")
    }

    @Test(.timeLimit(.minutes(1)))
    func updateObservedUpdateState() async {
        // GIVEN
        let cache = UpdaterStateCache()

        @ObservedUpdater(updaterObservable: cache) var observedState: KDC.UpdateState?
        let receivedValues = $observedState.receivedValues // Start to save the received values

        let initialState = KDC.UpdateState.Checking
        await cache.setUpdateState(initialState)

        _ = await receivedValues.first(where: { _ in true })

        let cachedState = await cache.getUpdateState()
        #expect(cachedState == initialState, "The cache should have been updated with the initial state")
        #expect(observedState == initialState, "The observed state should match the initial state")

        // WHEN
        let updatedState = KDC.UpdateState.Available
        await cache.setUpdateState(updatedState)

        // THEN
        _ = await receivedValues.first(where: { _ in true })

        let latestCachedState = await cache.getUpdateState()
        #expect(latestCachedState == updatedState, "The cache should have been updated with the new state")
        #expect(observedState == updatedState, "The observed state should match the updated one")
        #expect(observedState != initialState, "The observed state should not match the old one")
    }

    @Test(.timeLimit(.minutes(1)))
    func multipleUpdateStateChanges() async {
        // GIVEN
        let cache = UpdaterStateCache()

        @ObservedUpdater(updaterObservable: cache) var observedState: KDC.UpdateState?
        let receivedValues = $observedState.receivedValues // Start to save the received values

        #expect(observedState == nil, "UpdateState should initially be nil")

        // WHEN - Set multiple states in sequence
        let firstState = KDC.UpdateState.Checking
        await cache.setUpdateState(firstState)

        _ = await receivedValues.first(where: { _ in true })
        #expect(observedState == firstState, "The observed state should be checking")

        let secondState = KDC.UpdateState.Downloading
        await cache.setUpdateState(secondState)

        _ = await receivedValues.first(where: { _ in true })
        #expect(observedState == secondState, "The observed state should be downloading")

        let thirdState = KDC.UpdateState.Ready
        await cache.setUpdateState(thirdState)

        // THEN
        _ = await receivedValues.first(where: { _ in true })

        let finalCachedState = await cache.getUpdateState()
        #expect(finalCachedState == thirdState, "The cache should have the final state")
        #expect(observedState == thirdState, "The observed state should be ready")
    }

    @Test(.timeLimit(.minutes(1)))
    func setSameUpdateStateMultipleTimes() async {
        // GIVEN
        let cache = UpdaterStateCache()

        @ObservedUpdater(updaterObservable: cache) var observedState: KDC.UpdateState?
        let receivedValues = $observedState.receivedValues // Start to save the received values

        let expectedState = KDC.UpdateState.UpToDate
        await cache.setUpdateState(expectedState)

        _ = await receivedValues.first(where: { _ in true })

        #expect(observedState == expectedState, "The observed state should be upToDate")

        // WHEN - Set the same state again
        await cache.setUpdateState(expectedState)

        try? await Task.sleep(nanoseconds: 200_000_000) // Thanks to deduplication, receivedValues are not mutated

        // THEN - Value should still be correct
        let cachedState = await cache.getUpdateState()
        #expect(cachedState == expectedState, "The cache should still have the same state")
        #expect(observedState == expectedState, "The observed state should still be upToDate")
    }

    @Test(.timeLimit(.minutes(1)))
    func allUpdateStateValues() async {
        // GIVEN
        let cache = UpdaterStateCache()

        @ObservedUpdater(updaterObservable: cache) var observedState: KDC.UpdateState?
        let receivedValues = $observedState.receivedValues // Start to save the received values

        let states: [KDC.UpdateState] = [.UpToDate,
                                         .Checking,
                                         .Available,
                                         .ManualUpdateAvailable,
                                         .Downloading,
                                         .Ready,
                                         .CheckError,
                                         .DownloadError,
                                         .UpdateError,
                                         .NoUpdate,
                                         .Unknown,
                                         .EnumEnd].shuffled()

        for (index, expectedState) in states.enumerated() {
            // WHEN
            await cache.setUpdateState(expectedState)

            // THEN
            _ = await receivedValues.first(where: { _ in true })

            let cachedState = await cache.getUpdateState()
            #expect(cachedState == expectedState, "The cache should have state at index \(index)")
            #expect(observedState == expectedState, "The observed state should match at index \(index)")
        }
    }
}
