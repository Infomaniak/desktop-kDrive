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

public protocol VFSConversionCaching: Sendable {
    func beginConversion(synchroDbId: Int32) async -> UUID
    func finishConversion(synchroDbId: Int32, token: UUID) async
    func isConverting(synchroDbId: Int32) async -> Bool
    func removeConversion(synchroDbId: Int32) async
    func clear() async
}

public protocol VFSConversionCacheObservable: Sendable {
    var convertingSynchroIdsPublisher: AnyPublisher<Set<Int32>, Never> { get }
}

public extension VFSConversionCacheObservable {
    func isConvertingPublisher(synchroDbId: Int32) -> AnyPublisher<Bool, Never> {
        convertingSynchroIdsPublisher
            .map { $0.contains(synchroDbId) }
            .removeDuplicates()
            .eraseToAnyPublisher()
    }
}

/// Transient GUI operations, independent of server-backed synchronization data.
public actor VFSConversionCache: VFSConversionCaching, VFSConversionCacheObservable {
    private var activeConversions: [Int32: UUID] = [:]
    private nonisolated let conversionsSubject = CurrentValueSubject<Set<Int32>, Never>([])

    public nonisolated var convertingSynchroIdsPublisher: AnyPublisher<Set<Int32>, Never> {
        conversionsSubject.eraseToAnyPublisher()
    }

    public init() {}

    public func beginConversion(synchroDbId: Int32) -> UUID {
        let token = UUID()
        activeConversions[synchroDbId] = token
        publishConversions()
        return token
    }

    public func finishConversion(synchroDbId: Int32, token: UUID) {
        guard activeConversions[synchroDbId] == token else { return }
        removeConversion(synchroDbId: synchroDbId)
    }

    public func isConverting(synchroDbId: Int32) -> Bool {
        activeConversions[synchroDbId] != nil
    }

    public func removeConversion(synchroDbId: Int32) {
        guard activeConversions.removeValue(forKey: synchroDbId) != nil else { return }
        publishConversions()
    }

    public func clear() {
        activeConversions.removeAll()
        publishConversions()
    }

    private func publishConversions() {
        conversionsSubject.send(Set(activeConversions.keys))
    }
}
