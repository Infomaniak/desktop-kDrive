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

public typealias ConvertingSynchros = Set<Int32>

public typealias ConvertingSynchrosPublisher = AnyPublisher<ConvertingSynchros, Never>

public protocol VFSConversionStoreObservable: Sendable {
    var convertingSynchrosPublisher: ConvertingSynchrosPublisher { get }
}

public protocol VFSConversionStoring: Sendable {
    func conversionStarted(synchroDbId: Int32) async
    func conversionCompleted(synchroDbId: Int32) async
    func isConverting(synchroDbId: Int32) async -> Bool
}

public actor VFSConversionStore: VFSConversionStoring, VFSConversionStoreObservable {
    private var convertingSynchros: ConvertingSynchros = []

    private nonisolated let convertingSynchrosSubject = CurrentValueSubject<ConvertingSynchros, Never>([])

    public nonisolated var convertingSynchrosPublisher: ConvertingSynchrosPublisher {
        convertingSynchrosSubject.eraseToAnyPublisher()
    }

    public init() {}

    public func conversionStarted(synchroDbId: Int32) {
        let (isInserted, _) = convertingSynchros.insert(synchroDbId)

        guard isInserted else {
            return
        }
        notifyConvertingSynchrosChange()
    }

    public func conversionCompleted(synchroDbId: Int32) {
        guard convertingSynchros.remove(synchroDbId) != nil else {
            return
        }
        notifyConvertingSynchrosChange()
    }

    public func isConverting(synchroDbId: Int32) -> Bool {
        convertingSynchros.contains(synchroDbId)
    }

    private func notifyConvertingSynchrosChange() {
        convertingSynchrosSubject.send(convertingSynchros)
    }
}
