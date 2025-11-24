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

public struct RequestMock: Codable, Sendable {
    public let id: Int32
    public let num: RequestNum
}

public struct ReplyMock: Codable, Sendable {
    public let id: Int32
    public let num: RequestNum
}

public actor XPCServerMock: XPCGuiProtocol, XPCConnectionProvider {
    @InjectService var signalHandler: XPCSignalHandlerProtocol

    let encoder = JSONEncoder()
    let decoder = JSONDecoder()
    let cache = ServerCoherentCache()
    let requestCounter = AutoIncrementIDGenerator()
    let userDbIdGenerator = AutoIncrementIDGenerator()

    enum serverError: Error {
        case unsupported
    }

    public var guiConnection: XPCGuiProtocol {
        self
    }

    public init() {}

    @objc public nonisolated func processQuery(_ query: Data, callback: @escaping (Data) -> Void) {
        Task {
            do {
                let request = try decoder.decode(RequestMock.self, from: query)
                switch request.num {
                case .LOGIN_REQUESTTOKEN:
                    IKLogger.xpc.log("XPCServerMock: LOGIN_REQUESTTOKEN")
                    let userDbId = try await replySuccessWithUserDbId(callback: callback)
                    try await addUserInCacheAndSignalUpdate(userDbId: userDbId)

                default:
                    IKLogger.xpc.error("XPCServerMock: missing implementation for request \(request.num)")
                    throw serverError.unsupported
                }
            } catch {
                IKLogger.xpc.error("XPCServerMock: Kaput")
                try await replyDummyError(callback: callback)
            }
        }
    }

    func replySuccessWithUserDbId(callback: @escaping (Data) -> Void) async throws -> Int32 {
        let requestId = await requestCounter.nextID
        let userDbId: Int32 = await userDbIdGenerator.nextID
        let loginResponse = LoginResponse(userDbId: userDbId)
        let callbackMessage = CallbackMessage<LoginResponse>(code: .Ok, cause: .Unknown, id: requestId, body: loginResponse)
        let encodedCallback = try encoder.encode(callbackMessage)
        callback(encodedCallback)
        return requestId
    }

    func addUserInCacheAndSignalUpdate(userDbId: Int32) async throws {
        try await Task.sleep(nanoseconds: 1_000_000_000)
        let newUser = User(dbId: userDbId,
                           userId: Int32.random(in: 0 ... 1000),
                           name: "Jonh Appleseed",
                           email: "jonh.appleseed@apple.com",
                           accounts: [:],
                           availableDrives: [],
                           isConnected: true,
                           isStaff: true)
        await cache.addUser(newUser)

        let requestIdentifier = await requestCounter.nextID
        let userInfo = SignalParams<UserInfoSignal>(body: newUser.asUserInfoSignal)
        let updateUserSignal = SignalMessage<UserInfoSignal>(id: requestIdentifier,
                                                             num: SignalNum.USER_UPDATED,
                                                             params: userInfo)
        let encodedSignal = try encoder.encode(updateUserSignal)
        signalHandler.handleServerSignal(encodedSignal)
    }

    func replyDummyError(callback: @escaping (Data) -> Void) async throws {
        let requestId = await requestCounter.nextID
        let callbackMessage = CallbackMessage<EmptyResponse>(
            code: .SystemError,
            cause: .Unknown,
            id: requestId,
            body: EmptyResponse()
        )
        let encodedCallback = try encoder.encode(callbackMessage)
        callback(encodedCallback)
    }
}
