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

import AppKit
import CommonCrypto
import Foundation

@MainActor
public protocol WebBrowserLoginDelegate: AnyObject {
    func didCompleteLoginWith(code: String, verifier: String)
    func didFailLoginWith(error: Error)
}

public enum WebBrowserLoginError: Error {
    case invalidLoginURL
    case stateMismatch
}

@MainActor
public protocol WebBrowserLoginServiceable: AnyObject {
    func loginInDefaultBrowser(delegate: WebBrowserLoginDelegate)
    func didReceiveAuthorizationCode(code: String, state: String)
}

@MainActor
public final class WebBrowserLoginService: WebBrowserLoginServiceable {
    private let loginURL: URL
    private let clientId: String
    private let redirectURI: String

    private weak var delegate: WebBrowserLoginDelegate?

    private var codeVerifier: String?
    private var state: String?

    public nonisolated init(loginURL: URL, clientId: String, redirectURI: String) {
        self.loginURL = loginURL
        self.clientId = clientId
        self.redirectURI = redirectURI
    }

    public func loginInDefaultBrowser(delegate: WebBrowserLoginDelegate) {
        self.delegate = delegate

        let codeVerifier = Self.generateCodeVerifier()
        self.codeVerifier = codeVerifier

        let state = Self.generateState()
        self.state = state

        guard let url = makeLoginURL(codeChallenge: Self.generateCodeChallenge(from: codeVerifier), state: state) else {
            reset()
            delegate.didFailLoginWith(error: WebBrowserLoginError.invalidLoginURL)
            return
        }

        NSWorkspace.shared.open(url)
    }

    public func didReceiveAuthorizationCode(code: String, state: String) {
        guard let codeVerifier, let expectedState = self.state else { return }

        reset()

        guard state == expectedState else {
            delegate?.didFailLoginWith(error: WebBrowserLoginError.stateMismatch)
            return
        }

        delegate?.didCompleteLoginWith(code: code, verifier: codeVerifier)
    }

    private func reset() {
        codeVerifier = nil
        state = nil
    }

    // MARK: - URL building

    private func makeLoginURL(codeChallenge: String, state: String) -> URL? {
        var urlComponents = URLComponents(url: loginURL, resolvingAgainstBaseURL: true)
        urlComponents?.path = "/authorize"
        urlComponents?.queryItems = [
            URLQueryItem(name: "skipAutoRedirect", value: "true"),
            URLQueryItem(name: "response_type", value: "code"),
            URLQueryItem(name: "client_id", value: clientId),
            URLQueryItem(name: "redirect_uri", value: redirectURI),
            URLQueryItem(name: "code_challenge", value: codeChallenge),
            URLQueryItem(name: "code_challenge_method", value: "S256"),
            URLQueryItem(name: "state", value: state)
        ]

        return urlComponents?.url
    }

    // MARK: - PKCE

    private static func generateCodeVerifier() -> String {
        var buffer = [UInt8](repeating: 0, count: 32)
        _ = SecRandomCopyBytes(kSecRandomDefault, buffer.count, &buffer)
        return base64URLEncode(Data(buffer))
    }

    private static func generateCodeChallenge(from codeVerifier: String) -> String {
        guard let data = codeVerifier.data(using: .utf8) else {
            return ""
        }

        var buffer = [UInt8](repeating: 0, count: Int(CC_SHA256_DIGEST_LENGTH))
        data.withUnsafeBytes {
            _ = CC_SHA256($0.baseAddress, CC_LONG(data.count), &buffer)
        }

        return base64URLEncode(Data(buffer))
    }

    private static func generateState() -> String {
        var buffer = [UInt8](repeating: 0, count: 16)
        _ = SecRandomCopyBytes(kSecRandomDefault, buffer.count, &buffer)
        return base64URLEncode(Data(buffer))
    }

    private static func base64URLEncode(_ data: Data) -> String {
        return data.base64EncodedString()
            .replacingOccurrences(of: "+", with: "-")
            .replacingOccurrences(of: "/", with: "_")
            .replacingOccurrences(of: "=", with: "")
            .trimmingCharacters(in: .whitespaces)
    }
}
