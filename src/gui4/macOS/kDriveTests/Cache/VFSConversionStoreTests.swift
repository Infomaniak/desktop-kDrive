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
import Foundation
@testable import kDriveCore
import Testing

@Suite("VFSConversionStore Tests")
struct VFSConversionStoreTests {
    @Test(.timeLimit(.minutes(1)))
    func conversionStartedMarksSynchroAsConverting() async {
        // GIVEN
        let store = VFSConversionStore()
        let syncDbId = Int32.random(in: 0 ... 10000)
        #expect(await store.isConverting(synchroDbId: syncDbId) == false)

        // WHEN
        await store.conversionStarted(synchroDbId: syncDbId)

        // THEN
        #expect(await store.isConverting(synchroDbId: syncDbId) == true)
    }

    @Test(.timeLimit(.minutes(1)))
    func conversionCompletedClearsSynchro() async {
        // GIVEN
        let store = VFSConversionStore()
        let syncDbId = Int32.random(in: 0 ... 10000)
        await store.conversionStarted(synchroDbId: syncDbId)
        #expect(await store.isConverting(synchroDbId: syncDbId) == true)

        // WHEN
        await store.conversionCompleted(synchroDbId: syncDbId)

        // THEN
        #expect(await store.isConverting(synchroDbId: syncDbId) == false)
    }

    @Test(.timeLimit(.minutes(1)))
    func tracksMultipleSynchrosIndependently() async {
        // GIVEN
        let store = VFSConversionStore()
        let firstSyncDbId = Int32.random(in: 0 ... 5000)
        let secondSyncDbId = Int32.random(in: 5001 ... 10000)

        // WHEN
        await store.conversionStarted(synchroDbId: firstSyncDbId)
        await store.conversionStarted(synchroDbId: secondSyncDbId)
        await store.conversionCompleted(synchroDbId: firstSyncDbId)

        // THEN
        #expect(await store.isConverting(synchroDbId: firstSyncDbId) == false)
        #expect(await store.isConverting(synchroDbId: secondSyncDbId) == true)
    }

    @Test(.timeLimit(.minutes(1)))
    func publishesConvertingSynchrosChanges() async {
        // GIVEN
        let store = VFSConversionStore()
        let syncDbId = Int32.random(in: 0 ... 10000)
        let receivedValues = values(from: store.convertingSynchrosPublisher)

        // WHEN
        await store.conversionStarted(synchroDbId: syncDbId)

        // THEN
        let receivedConverting = await receivedValues.first(where: { $0.contains(syncDbId) })
        #expect(receivedConverting?.contains(syncDbId) == true)
    }

    @Test(.timeLimit(.minutes(1)))
    func replaysLatestConvertingSynchrosToNewSubscribers() async {
        // GIVEN
        let store = VFSConversionStore()
        let syncDbId = Int32.random(in: 0 ... 10000)
        await store.conversionStarted(synchroDbId: syncDbId)
        let receivedValues = values(from: store.convertingSynchrosPublisher)

        // WHEN
        let receivedConverting = await receivedValues.first(where: { _ in true })

        // THEN
        #expect(receivedConverting?.contains(syncDbId) == true)
    }

    private func values(from publisher: ConvertingSynchrosPublisher) -> AsyncStream<ConvertingSynchros> {
        AsyncStream { continuation in
            let cancellable = publisher.sink { converting in
                continuation.yield(converting)
            }

            continuation.onTermination = { _ in cancellable.cancel() }
        }
    }
}
