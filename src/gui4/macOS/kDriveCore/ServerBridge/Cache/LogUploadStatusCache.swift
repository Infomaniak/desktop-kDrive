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
import CppInterop

public struct LogUploadStatus: Equatable, Sendable {
    public let state: KDC.LogUploadState
    public let percentage: Int32

    public init(state: KDC.LogUploadState, percentage: Int32) {
        self.state = state
        self.percentage = percentage
    }

    init(signal: LogUploadStatusSignal) {
        self.init(state: signal.state, percentage: signal.percentage)
    }

    public var isCompleted: Bool {
        switch state {
        case .Success, .Failed, .Canceled:
            return true
        default:
            return false
        }
    }
}

public typealias LogUploadStatusPublisher = AnyPublisher<LogUploadStatus, Never>

public protocol LogUploadStatusCacheObservable: Sendable {
    var logUploadStatusPublisher: LogUploadStatusPublisher { get }
}

public protocol LogUploadStatusCaching: Sendable {
    func setLogUploadStatus(_ status: LogUploadStatus) async
    func getLogUploadStatus() async -> LogUploadStatus?
}

public actor LogUploadStatusCache: LogUploadStatusCaching, LogUploadStatusCacheObservable {
    private var logUploadStatus: LogUploadStatus?

    private nonisolated let logUploadStatusSubject = CurrentValueSubject<LogUploadStatus?, Never>(nil)

    public nonisolated var logUploadStatusPublisher: LogUploadStatusPublisher {
        logUploadStatusSubject
            .compactMap { $0 }
            .eraseToAnyPublisher()
    }

    public init() {}

    public func setLogUploadStatus(_ status: LogUploadStatus) {
        logUploadStatus = status
        notifyLogUploadStatusChange(status)
    }

    public func getLogUploadStatus() -> LogUploadStatus? {
        logUploadStatus
    }

    private func notifyLogUploadStatusChange(_ status: LogUploadStatus) {
        logUploadStatusSubject.send(status)
    }
}
