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

            var authRequestParams = AuthRequestParams.CreateForAuthorizationCodeRequest(App.Constants.Login.OAtuhClientId, App.Constants.Login.OAtuhRedirectUri);
            authRequestParams.CodeChallengeMethod = CodeChallengeMethodKind.S256;
            try
            {
                var authRequestResult = await OAuth2Manager
                    .RequestAuthWithParamsAsync(parentWindowId, App.Constants.Login.OAtuhAuthorizationEndpoint, authRequestParams)
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
