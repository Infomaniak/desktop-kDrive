using Microsoft.Security.Authentication.OAuth;
using Microsoft.UI;
using Microsoft.UI.Xaml;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive
{
    static class OAuthHelper
    {
        private static readonly string _clientId = "5EA39279-FF64-4BB8-A872-4A40B5786317";
        private static readonly Uri _redirectUri = new Uri("kdrive://auth-desktop");
        private static readonly Uri _authorizationEndpoint = new Uri("https://login.infomaniak.com/authorize");
        private static readonly Uri _tokenEndpoint = new Uri("https://login.infomaniak.com/token");

        public static async Task<string> GetToken(CancellationToken cancellationToken)
        {
            var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(((App)Application.Current).CurrentWindow);
            var parentWindowId = Win32Interop.GetWindowIdFromWindow(hWnd);

            var authRequestParams = AuthRequestParams.CreateForAuthorizationCodeRequest(_clientId, _redirectUri);
            authRequestParams.CodeChallengeMethod = CodeChallengeMethodKind.S256;

            try
            {
                var authRequestResult = await OAuth2Manager
                    .RequestAuthWithParamsAsync(parentWindowId, _authorizationEndpoint, authRequestParams)
                    .AsTask(cancellationToken);

                if (authRequestResult.Response is AuthResponse authResponse)
                {
                    Logger.Log(Logger.Level.Info, "OAuth authorization successful.");
                    return await DoTokenExchange(authResponse);
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

            return "";
        }


        private static async Task<string> DoTokenExchange(AuthResponse authResponse)
        {
            TokenRequestParams tokenRequestParams = TokenRequestParams.CreateForAuthorizationCodeRequest(authResponse);
            TokenRequestResult tokenRequestResult = await OAuth2Manager.RequestTokenAsync(_tokenEndpoint, tokenRequestParams);

            if (tokenRequestResult.Response is TokenResponse tokenResponse)
            {
                Logger.Log(Logger.Level.Info, "OAuth token exchange successful.");
                return tokenResponse.AccessToken;
            }
            else
            {
                TokenFailure tokenFailure = tokenRequestResult.Failure;
                Logger.Log(Logger.Level.Error, $"OAuth token exchange failed: {tokenFailure.Error}, {tokenFailure.ErrorDescription}");
                return "";
            }
        }
    }
}
