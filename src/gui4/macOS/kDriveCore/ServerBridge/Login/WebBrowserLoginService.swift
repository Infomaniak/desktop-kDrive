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
    case randomGenerationFailed
    case browserOpeningFailed
    case accessDenied
    case authorizationFailed(error: String, description: String?)
    case missingAuthorizationCode
}

@MainActor
public protocol WebBrowserLoginServiceable: AnyObject {
    func loginInDefaultBrowser(delegate: WebBrowserLoginDelegate)
    func cancelLogin()
    @discardableResult
    func handleRedirectURL(_ url: URL) -> Bool
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

        guard let codeVerifier = Self.generateRandomString(byteCount: 32),
              let state = Self.generateRandomString(byteCount: 16) else {
            reset()
            delegate.didFailLoginWith(error: WebBrowserLoginError.randomGenerationFailed)
            return
        }

        self.codeVerifier = codeVerifier
        self.state = state

        guard let url = makeLoginURL(codeChallenge: Self.generateCodeChallenge(from: codeVerifier), state: state) else {
            reset()
            delegate.didFailLoginWith(error: WebBrowserLoginError.invalidLoginURL)
            return
        }

        guard NSWorkspace.shared.open(url) else {
            reset()
            delegate.didFailLoginWith(error: WebBrowserLoginError.browserOpeningFailed)
            return
        }
    }

    @discardableResult
    public func handleRedirectURL(_ url: URL) -> Bool {
        guard let urlComponents = URLComponents(url: url, resolvingAgainstBaseURL: false),
              isRedirectURL(urlComponents) else {
            return false
        }

        guard let delegate, let codeVerifier, let expectedState = state else {
            IKLogger.general.info("Ignoring login redirect: no login in progress")
            return false
        }

        let queryItems = urlComponents.queryItems ?? []
        func queryValue(_ name: String) -> String? {
            queryItems.first { $0.name == name }?.value
        }

        guard queryValue("state") == expectedState else {
            IKLogger.general.warning("Ignoring login redirect with unexpected state")
            return false
        }

        reset()

        if let error = queryValue("error") {
            let loginError: WebBrowserLoginError = error == "access_denied"
                ? .accessDenied
                : .authorizationFailed(error: error, description: queryValue("error_description"))
            delegate.didFailLoginWith(error: loginError)
        } else if let code = queryValue("code"), !code.isEmpty {
            delegate.didCompleteLoginWith(code: code, verifier: codeVerifier)
        } else {
            delegate.didFailLoginWith(error: WebBrowserLoginError.missingAuthorizationCode)
        }

        return true
    }

    public func cancelLogin() {
        reset()
    }

    private func reset() {
        codeVerifier = nil
        state = nil
        delegate = nil
    }

    private func isRedirectURL(_ urlComponents: URLComponents) -> Bool {
        guard let redirectComponents = URLComponents(string: redirectURI) else { return false }

        return urlComponents.scheme?.lowercased() == redirectComponents.scheme?.lowercased()
            && urlComponents.host?.lowercased() == redirectComponents.host?.lowercased()
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

    private static func generateRandomString(byteCount: Int) -> String? {
        var buffer = [UInt8](repeating: 0, count: byteCount)
        guard SecRandomCopyBytes(kSecRandomDefault, buffer.count, &buffer) == errSecSuccess else {
            IKLogger.general.error("Failed to generate secure random bytes for login")
            return nil
        }
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

    private static func base64URLEncode(_ data: Data) -> String {
        return data.base64EncodedString()
            .replacingOccurrences(of: "+", with: "-")
            .replacingOccurrences(of: "/", with: "_")
            .replacingOccurrences(of: "=", with: "")
            .trimmingCharacters(in: .whitespaces)
    }
}
