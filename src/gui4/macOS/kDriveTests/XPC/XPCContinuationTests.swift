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
@testable import kDriveCore
import Testing

struct XPCContinuationTests {
    enum TestError: Error {
        case proxyFailure
    }

    @Test func ignoresProxyErrorAfterReply() async throws {
        let result: String = try await withCheckedThrowingContinuation { continuation in
            let continuation = XPCContinuation(continuation)
            Task {
                await continuation.resume(returning: "reply")
                await continuation.resume(throwing: TestError.proxyFailure)
            }
        }

        #expect(result == "reply")
    }

    @Test func ignoresReplyAfterProxyError() async throws {
        do {
            let _: String = try await withCheckedThrowingContinuation { continuation in
                let continuation = XPCContinuation<String>(continuation)
                Task {
                    await continuation.resume(throwing: TestError.proxyFailure)
                    await continuation.resume(returning: "late reply")
                }
            }
            Issue.record("Expected the proxy error to be thrown")
        } catch {
            #expect(error is TestError)
        }
    }
}
