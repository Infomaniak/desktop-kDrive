/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

import AppKit
import kDriveCore

@MainActor
final class ApplicationShutdown {
    private enum State {
        case running
        case waitingForServer
        case readyToTerminate
    }

    private var state: State = .running
    private let connectionLifecycle: XPCConnectionLifecycle
    private let quitServer: @MainActor () async -> Void
    private let reply: @MainActor () -> Void

    init(connectionLifecycle: XPCConnectionLifecycle,
         quitServer: @escaping @MainActor () async -> Void,
         reply: @escaping @MainActor () -> Void) {
        self.connectionLifecycle = connectionLifecycle
        self.quitServer = quitServer
        self.reply = reply
    }

    func requestTermination(notifyServer: Bool) -> NSApplication.TerminateReply {
        switch state {
        case .waitingForServer:
            return .terminateLater
        case .readyToTerminate:
            return .terminateNow
        case .running:
            state = .waitingForServer
        }

        connectionLifecycle.beginShutdown()
        guard notifyServer else {
            connectionLifecycle.finishShutdown()
            state = .readyToTerminate
            return .terminateNow
        }

        Task {
            await quitServer()
            connectionLifecycle.finishShutdown()
            state = .readyToTerminate
            reply()
        }
        return .terminateLater
    }
}
