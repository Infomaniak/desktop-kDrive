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

import Foundation
@testable import InfomaniakDI
@testable import kDrive
@testable import kDriveCore
import Testing

private struct UnencodableRequest: Encodable {
    func encode(to encoder: any Encoder) throws {
        throw EncodingError.invalidValue("private-name", .init(codingPath: [], debugDescription: "private-path"))
    }
}

private actor PartiallyFailingSyncCreator: SyncCreator {
    private(set) var attempts = 0

    func create(from sync: NewSyncCandidate) async throws -> SyncInfo {
        attempts += 1
        if attempts > 1 {
            throw NSError(domain: NSCocoaErrorDomain, code: 513, userInfo: [NSFilePathErrorKey: "/private-name/private-folder"])
        }
        return try JSONDecoder().decode(SyncInfo.self, from: Data(
            #"{"dbId":123,"driveDbId":456,"localPath":"","supportVfs":false,"targetNodeId":"","targetPath":"","virtualFileMode":0}"#.utf8
        ))
    }

    func preferredLocalPath(for driveName: String) async throws -> URL {
        URL(fileURLWithPath: "/private-name/private-folder")
    }
}

extension SharedDITests {
    @Suite
    struct XPCDiagnosticsTests {
        private func withLogger(_ operation: (LogService, InMemoryLogFileWriter, SpySentryLogReporter) async throws -> Void) async throws {
            let resolver = SimpleResolver.sharedResolver
            let identifier = resolver.buildIdentifier(type: LogService.self)
            let previousFactory = resolver.factories[identifier]
            let previousService = resolver.store[identifier]
            defer {
                resolver.factories[identifier] = previousFactory
                resolver.store[identifier] = previousService
            }
            let writer = InMemoryLogFileWriter()
            let reporter = SpySentryLogReporter()
            let service = LogService(fileWriter: writer, sentryReporter: reporter, minimumFileLevel: .info)
            resolver.store.removeValue(forKey: identifier)
            resolver.store(factory: Factory(type: LogService.self) { _, _ in service })
            try await operation(service, writer, reporter)
        }

        @Test("Encoding and header failures are observable without exposing request or response contents")
        func reportsPreflightFailures() async throws {
            try await withLogger { service, writer, reporter in
                let recorder = RequestRecorder()
                let provider = RecordingXPCConnectionProvider(recorder: recorder, responseData: Data("private-response".utf8))
                let fetcher = XPCQueryFetcher(xpcConnectionProvider: provider)
                do {
                    try await fetcher.query(UnencodableRequest(), responseType: CallbackMessage<EmptyResponse>.self)
                    Issue.record("Expected encoding failure")
                } catch {
                    #expect(error is EncodingError)
                }
                #expect(await recorder.requestData == nil)
                do {
                    try await fetcher.query(EmptyQuery(), responseType: CallbackMessage<EmptyResponse>.self)
                    Issue.record("Expected header failure")
                } catch {
                    #expect(error is DecodingError)
                }
                service.flush()
                #expect(writer.lines.count == 2)
                #expect(reporter.capturedEvents.count == 2)
                #expect(writer.lines.contains { $0.contains("request encoding failed") })
                #expect(writer.lines.contains { $0.contains("callback header decoding failed") })
                #expect(!reporter.breadcrumbs.contains { $0.message.contains("private-") })
            }
        }

        @Test("Routine traffic stays below info while canceled and transient callbacks avoid error captures")
        func classifiesCallbackOutcomes() async throws {
            try await withLogger { service, writer, reporter in
                for code in [KDC.ExitCode.Ok, .OperationCanceled, .RateLimited, .NetworkError, .DataError] {
                    let response = CallbackMessage<EmptyResponse>(code: code, cause: .Unknown, id: 42, body: EmptyResponse())
                    let provider = try MCKXPCConnectionProviderWithData(responseData: JSONEncoder().encode(response))
                    do {
                        try await XPCQueryFetcher(xpcConnectionProvider: provider).query(
                            EmptyQuery(), responseType: CallbackMessage<EmptyResponse>.self
                        )
                        #expect(code == .Ok)
                    } catch {
                        #expect(error is CallbackError)
                        #expect(code != .Ok)
                    }
                }
                service.flush()
                #expect(writer.lines.count == 4)
                #expect(reporter.capturedEvents.count == 1)
                #expect(reporter.capturedEvents.first?.message.contains("DataError") == true)
                #expect(reporter.breadcrumbs.filter { $0.level == .warning }.count == 2)
            }
        }

        @Test("Partial onboarding reports created and remaining counts before advancing without logging candidate paths")
        func reportsPartialOnboarding() async throws {
            try await withLogger { service, writer, _ in
                let creator = PartiallyFailingSyncCreator()
                let resolver = SimpleResolver.sharedResolver
                let identifier = resolver.buildIdentifier(type: SyncCreator.self)
                let previousFactory = resolver.factories[identifier]
                let previousService = resolver.store[identifier]
                defer {
                    resolver.factories[identifier] = previousFactory
                    resolver.store[identifier] = previousService
                }
                resolver.store.removeValue(forKey: identifier)
                resolver.store(factory: Factory(type: SyncCreator.self) { _, _ in creator })

                let (finished, continuation) = AsyncStream<Void>.makeStream()
                let viewModel = await MainActor.run {
                    let coordinator = OnboardingFlowCoordinator(user: nil, steps: [.synchronization], initialStep: .synchronization) {
                        continuation.yield(())
                    }
                    let candidate = NewSyncCandidate(
                        origin: .storedDrive(CacheData.expectedDrive), remoteFolder: .kDriveRoot,
                        localFolder: URL(fileURLWithPath: "/private-name/private-folder"), blackList: [], useLightSync: false
                    )
                    coordinator.synchronizations = [candidate, candidate, candidate]
                    let viewModel = SynchronizationViewModel(flowCoordinator: coordinator)
                    viewModel.createSynchronizations()
                    return viewModel
                }
                _ = await finished.first { _ in true }
                #expect(await creator.attempts == 2)
                #expect(await viewModel.isShowingError)
                service.flush()
                #expect(writer.lines.contains { $0.contains("syncDbId=123 driveDbId=456") })
                #expect(writer.lines.contains { $0.contains("created=1 remaining=2 advancing=true") })
                #expect(!writer.lines.contains { $0.contains("creation completed") || $0.contains("private-") })
            }
        }
    }
}
