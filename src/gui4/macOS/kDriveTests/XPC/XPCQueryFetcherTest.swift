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
@testable import kDriveCore
import Testing

// TODO: Test OK + ErrorCode + Corrupted

class MCKXPCGuiProtocol: XPCGuiProtocol {
    private let decoder = JSONDecoder()

    let payloadFileName: String

    init(payloadFileName: String) {
        self.payloadFileName = payloadFileName
    }

    var responseData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: payloadFileName, withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    func processQuery(_ query: Data, callback: @escaping (Data) -> Void) {
        callback(responseData)
    }
}

class MCKXPCGuiProtocolWithData: XPCGuiProtocol {
    init(responseData: Data) {
        self.responseData = responseData
    }

    let responseData: Data

    func processQuery(_ query: Data, callback: @escaping (Data) -> Void) {
        callback(responseData)
    }
}

struct MCKXPCConnectionProvider: XPCConnectionProvider {
    var guiConnectionState: kDriveCore.XPCConnectionState = .notConnected

    var guiConnectionStatePublisher: AnyPublisher<kDriveCore.XPCConnectionState, Never> = Just(.connected).eraseToAnyPublisher()

    let payloadFileName: String

    var guiConnection: XPCGuiProtocol {
        get async throws {
            MCKXPCGuiProtocol(payloadFileName: payloadFileName)
        }
    }
}

struct MCKXPCConnectionProviderWithData: XPCConnectionProvider {
    var guiConnectionState: kDriveCore.XPCConnectionState = .notConnected

    var guiConnectionStatePublisher: AnyPublisher<kDriveCore.XPCConnectionState, Never> = Just(.connected).eraseToAnyPublisher()

    let responseData: Data

    var guiConnection: XPCGuiProtocol {
        get async throws {
            MCKXPCGuiProtocolWithData(responseData: responseData)
        }
    }
}

typealias SendableCodable = Codable & Sendable

struct XPCQueryFetcherTests {
    static let allResponseTypes: [any SendableCodable.Type] = [
        // UtilityJobs responses
        CallbackMessage<UtilityGetAppStateResponse>.self,
        CallbackMessage<UtilityHasLaunchOnStartupResponse>.self,
        CallbackMessage<UtilityHasSystemLaunchOnStartupResponse>.self,
        CallbackMessage<EmptyResponse>.self,
        CallbackMessage<UtilityIsPathValidForNewSyncResponse>.self,
        CallbackMessage<UtilityGoodPathNewSyncResponse>.self,
        CallbackMessage<UtilityBestVFSResponse>.self,
        // SyncJobs responses
        CallbackMessage<SyncInfoList>.self,
        CallbackMessage<SyncStatusResponse>.self,
        CallbackMessage<SyncInfoSingle>.self,
        CallbackMessage<LinkResponse>.self,
        // AccountJobs responses
        CallbackMessage<AccountListResponse>.self,
        // DriveJobs responses
        CallbackMessage<DriveInfoListResponse>.self,
        CallbackMessage<DriveListResponse>.self,
        // UserJobs responses
        CallbackMessage<UserDbIdsListResponse>.self,
        CallbackMessage<UserInfoListResponse>.self,
        // NodeJobs responses
        CallbackMessage<NodePathResponse>.self,
        CallbackMessage<NodeInfoResponse>.self,
        CallbackMessage<NodeSubfoldersResponse>.self,
        CallbackMessage<NodeSizeResponse>.self,
        CallbackMessage<CreateMissingFoldersResponse>.self
    ]

    @Test func decodingNoErrorResponse() async throws {
        // GIVEN
        let mockedConnectionProvider = MCKXPCConnectionProvider(payloadFileName: "AnyValidJobResponse")
        let queryFetcher = XPCQueryFetcher(xpcConnectionProvider: mockedConnectionProvider)
        let query = EmptyQuery()

        // WHEN
        let decodedMessage = try await queryFetcher.query(query, responseType: CallbackMessage<EmptyResponse>.self)

        // THEN
        #expect(decodedMessage.code == .Ok)
        #expect(decodedMessage.cause == .Unknown)
    }

    @Test(arguments: allResponseTypes)
    func decodingSomeErrorResponse(queryType: any SendableCodable.Type) async throws {
        // GIVEN
        let errorCallback = CallbackMessage<EmptyResponse>(
            code: KDC.ExitCode.BackError,
            cause: KDC.ExitCause.QuotaExceeded,
            id: Int32.random(in: Int32.min ... Int32.max),
            body: EmptyResponse()
        )

        let encoder = JSONEncoder()
        let data = try encoder.encode(errorCallback)
        let mockedConnectionProvider = MCKXPCConnectionProviderWithData(responseData: data)
        let queryFetcher = XPCQueryFetcher(xpcConnectionProvider: mockedConnectionProvider)
        let query = EmptyQuery()

        // WHEN
        do {
            try await queryFetcher.query(query, responseType: queryType)

            // THEN
            Issue.record("We should throw")
        }

        catch {
            guard let error = error as? CallbackError else {
                Issue.record("unexpected error \(error)")
                return
            }
            #expect(error == .serverError(code: KDC.ExitCode.BackError, cause: KDC.ExitCause.QuotaExceeded))
        }
    }

    @Test func corruptedResponse() async throws {
        // GIVEN
        let mockedConnectionProvider = MCKXPCConnectionProvider(payloadFileName: "ParsingError")
        let queryFetcher = XPCQueryFetcher(xpcConnectionProvider: mockedConnectionProvider)
        let query = EmptyQuery()

        // WHEN
        do {
            try await queryFetcher.query(query, responseType: CallbackMessage<AccountListResponse>.self)

            // THEN
            Issue.record("We should throw")
        }

        catch {
            guard let error = error as? XPCQueryFetcher.QueryError else {
                Issue.record("unexpected error \(error)")
                return
            }
            let expectedErrorValue: Bool
            if case .unableToDecodeReply = error {
                expectedErrorValue = true
            } else {
                expectedErrorValue = false
            }

            #expect(expectedErrorValue, "unexpected error \(error)")
        }
    }
}
