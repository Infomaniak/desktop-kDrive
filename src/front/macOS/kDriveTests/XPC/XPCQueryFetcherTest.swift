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

@testable import kDriveCore
import Testing

// TODO: Test OK + ErrorCode + Corrupted

class MCKXPCGuiProtocol: XPCGuiProtocol {
    private let decoder = JSONDecoder()

    let payloadFileName: String

    init(payloadFileName: String) {
        self.payloadFileName = payloadFileName
    }

    var errorData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: payloadFileName, withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    func processQuery(_ query: Data, callback: @escaping (Data) -> Void) {
        callback(errorData)
    }
}

struct MCKXPCConnectionProvider: XPCConnectionProvider {
    let payloadFileName: String

    var guiConnection: XPCGuiProtocol {
        get async throws {
            MCKXPCGuiProtocol(payloadFileName: payloadFileName)
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
        CallbackMessage<PublicLinkResponse>.self,
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
        CallbackMessage<MissingFolderResponse>.self
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
        let mockedConnectionProvider = MCKXPCConnectionProvider(payloadFileName: "QuotaExceededError")
        let queryFetcher = XPCQueryFetcher(xpcConnectionProvider: mockedConnectionProvider)
        let query = EmptyQuery()

        // WHEN
        do {
            _ = try await queryFetcher.query(query, responseType: queryType)

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
            _ = try await queryFetcher.query(query, responseType: CallbackMessage<AccountListResponse>.self)

            // THEN
            Issue.record("We should throw")
        }

        catch {
            guard let error = error as? XPCQueryFetcher.QueryError else {
                Issue.record("unexpected error \(error)")
                return
            }
            let expectedErrorValue: Bool = {
                if case .unableToDecodeReply = error {
                    true
                } else {
                    false
                }
            }()
            #expect(expectedErrorValue, "unexpected error \(error)")
        }
    }
}
