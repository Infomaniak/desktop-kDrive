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
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;

namespace Infomaniak.kDrive
{
    /// <summary>
    /// Diagnostics for the kdrive://auth-desktop OAuth protocol activation.
    /// <para>
    /// <see cref="OAuth2Manager.CompleteAuthRequest(Uri)"/> only returns a bool, so on failure the app cannot tell
    /// why it failed. The OAuth callback URI shape and the state of the inter-process pipe used by the
    /// Windows App SDK are the two discriminating facts: without a usable 'state' parameter in the callback URI,
    /// CompleteAuthRequest gives up before touching the pipe; with a state, it can only fail while opening or
    /// writing the pipe named after sha256(state).
    /// </para>
    /// <para>
    /// The authorization code is a credential and is never logged; only its presence and length are reported.
    /// </para>
    /// </summary>
    internal static class OAuthCallbackDiagnostics
    {
        private const string _pipeNamePrefix = @"\\.\pipe\oauth\";

        /// <summary>
        /// Describes the callback URI without any secret value: param names, state and code presence/length.
        /// </summary>
        public static string Describe(Uri callbackUri)
        {
            var uriParams = ParseUrlEncoded(callbackUri.Query);
            var stateEntry = uriParams.FirstOrDefault(p => p.Name == "state");
            var codeEntry = uriParams.FirstOrDefault(p => p.Name == "code");
            if (stateEntry.Name is null)
            {
                // WinAppSDK also accepts the state in the URI fragment
                var fragmentParams = ParseUrlEncoded(callbackUri.Fragment);
                stateEntry = fragmentParams.FirstOrDefault(p => p.Name == "state");
            }

            var paramNames = uriParams.Select(p => p.Name).ToList();
            string stateDescription = stateEntry.Name is null ? "state=MISSING" : $"state=present ({stateEntry.Value.Length} chars)";
            string codeDescription = codeEntry.Name is null ? "code=missing" : $"code=present ({codeEntry.Value.Length} chars)";
            return $"{callbackUri.Scheme}://{callbackUri.Host}, params=[{string.Join(", ", paramNames)}], {stateDescription}, {codeDescription}";
        }

        /// <summary>
        /// Describes an activation argument that failed to parse. Only its length and the protocol prefix are
        /// reported, so that no partial credential can leak into the logs.
        /// </summary>
        public static string DescribeUnparseable(string callbackArg, Exception parseError)
        {
            string prefix = callbackArg.Length > 0 ? callbackArg.Split('?')[0] : "<empty>";
            return $"length={callbackArg.Length}, prefix='{prefix}', parse error: {parseError.Message}";
        }

        /// <summary>
        /// Returns the full diagnostic for a failed <see cref="OAuth2Manager.CompleteAuthRequest(Uri)"/> call:
        /// the URI description, plus a probe of the state-named pipe when a state is present.
        /// </summary>
        public static string Diagnose(Uri callbackUri)
        {
            if (!TryGetState(callbackUri, out string? state))
            {
                return $"{Describe(callbackUri)}; no pipe probe possible without a state";
            }

            return $"{Describe(callbackUri)}; pipe probe: {ProbeStatePipe(state)}";
        }

        public static bool TryGetState(Uri callbackUri, out string? state)
        {
            state = ParseUrlEncoded(callbackUri.Query).FirstOrDefault(p => p.Name == "state").Value;
            if (string.IsNullOrEmpty(state))
            {
                state = ParseUrlEncoded(callbackUri.Fragment).FirstOrDefault(p => p.Name == "state").Value;
            }
            return !string.IsNullOrEmpty(state);
        }

        /// <summary>
        /// Recomputes the pipe name used by the Windows App SDK OAuth2Manager (sha256 of the state, base64 encoded)
        /// and reports the result of opening it. Opening is non-destructive: the pipe server simply reconnects and
        /// keeps waiting when the probe closes without writing anything.
        /// </summary>
        public static string ProbeStatePipe(string state)
        {
            string pipeName = ComputeStatePipeName(state);
            IntPtr handle = CreateFileW(pipeName, GENERIC_WRITE, 0, IntPtr.Zero, OPEN_EXISTING, 0, IntPtr.Zero);
            if (handle != INVALID_HANDLE_VALUE)
            {
                CloseHandle(handle);
                return $"pipe '{pipeName}' opened successfully (probe closed immediately); CompleteAuthRequest likely failed at a later stage, e.g. writing the response";
            }

            int error = Marshal.GetLastWin32Error();
            string hint = error switch
            {
                ERROR_FILE_NOT_FOUND => " (ERROR_FILE_NOT_FOUND: no pipe for this state - original request already completed, canceled, or from another session)",
                ERROR_ACCESS_DENIED => " (ERROR_ACCESS_DENIED: pipe exists but cannot be opened - another client is connected or a same-named pipe already existed)",
                ERROR_PIPE_BUSY => " (ERROR_PIPE_BUSY: pipe busy)",
                _ => ""
            };
            return $"pipe '{pipeName}' open failed, Win32 error {error}{hint}";
        }

        public static string ComputeStatePipeName(string state)
        {
            byte[] hash = SHA256.HashData(Encoding.UTF8.GetBytes(state));
            return _pipeNamePrefix + Convert.ToBase64String(hash);
        }

        private static IEnumerable<(string? Name, string Value)> ParseUrlEncoded(string? encodedParams)
        {
            // Trims the leading '?' of a query or '#' of a fragment
            string? trimmedParams = encodedParams?.TrimStart('?', '#');
            if (string.IsNullOrEmpty(trimmedParams))
            {
                yield break;
            }

            foreach (string pair in trimmedParams.Split('&', StringSplitOptions.RemoveEmptyEntries))
            {
                int separatorIndex = pair.IndexOf('=');
                string name = separatorIndex < 0 ? pair : pair[..separatorIndex];
                string value = separatorIndex < 0 ? "" : pair[(separatorIndex + 1)..].Replace('+', ' ');
                yield return (Uri.UnescapeDataString(name), Uri.UnescapeDataString(value));
            }
        }

        private const uint GENERIC_WRITE = 0x40000000;
        private const uint OPEN_EXISTING = 3;
        private const int ERROR_FILE_NOT_FOUND = 2;
        private const int ERROR_ACCESS_DENIED = 5;
        private const int ERROR_PIPE_BUSY = 231;
        private static readonly IntPtr INVALID_HANDLE_VALUE = new(-1);

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode, ExactSpelling = true)]
        private static extern IntPtr CreateFileW(string lpFileName, uint dwDesiredAccess, uint dwShareMode,
            IntPtr lpSecurityAttributes, uint dwCreationDisposition, uint dwFlagsAndAttributes, IntPtr hTemplateFile);

        [DllImport("kernel32.dll", SetLastError = true, ExactSpelling = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool CloseHandle(IntPtr hObject);
    }
}
