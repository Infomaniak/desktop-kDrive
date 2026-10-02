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
@testable import kDriveCore
import Testing

private struct UnencodableRequest: Encodable {
    func encode(to encoder: any Encoder) throws {
        throw EncodingError.invalidValue("private-name", .init(codingPath: [], debugDescription: "private-path"))
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
    }
}
