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

import Foundation
import InfomaniakDI

struct UtilitySignalHandler {
    private let decoder = JSONDecoder()
    @LazyInjectService private var coherentCache: CoherentCache

    func handleError(_ signal: Data) async throws {
        guard let errorInfoSignal = try? decoder.decode(SignalMessage<ErrorInfoSignal>.self, from: signal) else {
            throw SignalError.unableToGetErrorInfoFromSignal
        }

        let errorInfo = ErrorInfo(errorInfoMetadata: errorInfoSignal.body.errorInfo)
        try await coherentCache.addOrUpdateError(errorInfo)
    }

    func handleErrorRemoved(_ signal: Data) async throws {
        guard let errorRemovedSignal = try? decoder.decode(SignalMessage<ErrorRemovedSignal>.self, from: signal) else {
            throw SignalError.unableToGetErrorRemovedFromSignal
        }

        let errorDbId = errorRemovedSignal.body.errorDbId
        try await coherentCache.removeError(errorDbId)
    }

    func handleErrorCleared(_ signal: Data) async throws {
        await coherentCache.clearErrors()
    }
}
