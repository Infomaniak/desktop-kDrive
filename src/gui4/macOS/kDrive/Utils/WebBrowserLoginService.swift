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
import InfomaniakLogin

@MainActor
protocol WebBrowserLoginServiceable: AnyObject {
    func loginInDefaultBrowser(delegate: InfomaniakLoginDelegate)

    @discardableResult
    func handleRedirectURI(_ url: URL) -> Bool
}

@MainActor
final class WebBrowserLoginService: WebBrowserLoginServiceable {
    private let config: InfomaniakLogin.Config

    private weak var delegate: InfomaniakLoginDelegate?

    private var codeVerifier: String?
    private var state: String?

    nonisolated init(config: InfomaniakLogin.Config) {
        self.config = config
    }

    func loginInDefaultBrowser(delegate: InfomaniakLoginDelegate) {
        self.delegate = delegate

        let codeVerifier = generateCodeVerifier()
        self.codeVerifier = codeVerifier

        let state = generateState()
        self.state = state

        guard let loginURL = makeLoginURL(codeChallenge: generateCodeChallenge(from: codeVerifier), state: state) else {
            reset()
            delegate.didFailLoginWith(error: InfomaniakLoginError.invalidUrl)
            return
        }

        NSWorkspace.shared.open(loginURL)
    }

    @discardableResult
    func handleRedirectURI(_ url: URL) -> Bool {
        guard isRedirectURI(url), let codeVerifier, let expectedState = state else { return false }

        let queryItems = URLComponents(url: url, resolvingAgainstBaseURL: false)?.queryItems
        let returnedState = queryItems?.first { $0.name == "state" }?.value

        reset()

        guard returnedState == expectedState else {
            delegate?.didFailLoginWith(error: InfomaniakLoginError.accessDenied)
            return true
        }

        if let code = queryItems?.first(where: { $0.name == "code" })?.value {
            delegate?.didCompleteLoginWith(code: code, verifier: codeVerifier)
        } else {
            delegate?.didFailLoginWith(error: InfomaniakLoginError.accessDenied)
        }

        return true
    }

    private func reset() {
        codeVerifier = nil
        state = nil
    }

    // MARK: - URL building

    private func isRedirectURI(_ url: URL) -> Bool {
        guard let redirectComponents = URLComponents(string: config.redirectURI),
              let components = URLComponents(url: url, resolvingAgainstBaseURL: false) else {
            return false
        }

        return components.scheme?.caseInsensitiveCompare(redirectComponents.scheme ?? "") == .orderedSame
            && components.host?.caseInsensitiveCompare(redirectComponents.host ?? "") == .orderedSame
    }

    private func makeLoginURL(codeChallenge: String, state: String) -> URL? {
        var urlComponents = URLComponents(url: config.loginURL, resolvingAgainstBaseURL: true)
        urlComponents?.path = "/authorize"
        urlComponents?.queryItems = [
            URLQueryItem(name: "skipAutoRedirect", value: "true"),
            URLQueryItem(name: "response_type", value: config.responseType.rawValue),
            URLQueryItem(name: "client_id", value: config.clientId),
            URLQueryItem(name: "redirect_uri", value: config.redirectURI),
            URLQueryItem(name: "code_challenge", value: codeChallenge),
            URLQueryItem(name: "code_challenge_method", value: config.hashModeShort),
            URLQueryItem(name: "state", value: state)
        ]

        return urlComponents?.url
    }

    // MARK: - PKCE

    private func generateCodeVerifier() -> String {
        var buffer = [UInt8](repeating: 0, count: 32)
        _ = SecRandomCopyBytes(kSecRandomDefault, buffer.count, &buffer)
        return base64URLEncode(Data(buffer))
    }

    private func generateCodeChallenge(from codeVerifier: String) -> String {
        guard let data = codeVerifier.data(using: .utf8) else {
            return ""
        }

        var buffer = [UInt8](repeating: 0, count: Int(CC_SHA256_DIGEST_LENGTH))
        data.withUnsafeBytes {
            _ = CC_SHA256($0.baseAddress, CC_LONG(data.count), &buffer)
        }

        return base64URLEncode(Data(buffer))
    }

    private func generateState() -> String {
        var buffer = [UInt8](repeating: 0, count: 16)
        _ = SecRandomCopyBytes(kSecRandomDefault, buffer.count, &buffer)
        return base64URLEncode(Data(buffer))
    }

    private func base64URLEncode(_ data: Data) -> String {
        return data.base64EncodedString()
            .replacingOccurrences(of: "+", with: "-")
            .replacingOccurrences(of: "/", with: "_")
            .replacingOccurrences(of: "=", with: "")
            .trimmingCharacters(in: .whitespaces)
    }
}
