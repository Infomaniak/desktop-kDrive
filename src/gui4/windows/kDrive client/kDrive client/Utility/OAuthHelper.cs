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
using Microsoft.Security.Authentication.OAuth;
using Microsoft.UI;
using Microsoft.UI.Xaml;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive
{
    static class OAuthHelper
    {
        public struct OAuthResult
        {
            public string Code;
            public string CodeVerifier;
        }
        public static async Task<OAuthResult> GetCode(CancellationToken cancellationToken)
        {
            var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(((App)Application.Current).CurrentWindow);
            var parentWindowId = Win32Interop.GetWindowIdFromWindow(hWnd);

            var authRequestParams = AuthRequestParams.CreateForAuthorizationCodeRequest(App.Constants.Login.OAuthClientId, App.Constants.Login.OAuthRedirectUri);
            authRequestParams.CodeChallengeMethod = CodeChallengeMethodKind.S256;
            try
            {
                var authRequestResult = await OAuth2Manager
                    .RequestAuthWithParamsAsync(parentWindowId, App.Constants.Login.OAuthAuthorizationEndpoint, authRequestParams)
                    .AsTask(cancellationToken);

                if (authRequestResult.Response is AuthResponse authResponse)
                {
                    Logger.Log(Logger.Level.Info, "OAuth authorization successful.");
                    TokenRequestParams tokenRequestParams = TokenRequestParams.CreateForAuthorizationCodeRequest(authResponse);
                    return new OAuthResult { Code = authResponse.Code, CodeVerifier = tokenRequestParams.CodeVerifier };
                }

                if (authRequestResult.Failure is AuthFailure authFailure)
                {
                    Logger.Log(Logger.Level.Error,
                        $"OAuth authorization failed: {authFailure.Error}, {authFailure.ErrorDescription}");
                }
            }
            catch (OperationCanceledException)
            {
                Logger.Log(Logger.Level.Warning, "OAuth authorization canceled by user.");
                throw;
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Unexpected OAuth error: {ex}");
                throw;
            }
            finally
            {
                Utility.BringCurrentWindowToFront();
                Logger.Log(Logger.Level.Info, "OAuth authorization process completed.");
            }

            return new OAuthResult { Code = "", CodeVerifier = "" };
        }
    }
}
