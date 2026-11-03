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
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;

namespace Infomaniak.kDrive.Analytics
{
    internal enum UserAgentArchitecture
    {
        Unknown,
        X86,
        X64,
        Arm,
        Arm64
    }

    internal sealed record UserAgentInfo(
        string ProductName,
        string ProductVersion,
        string OperatingSystemVersion,
        UserAgentArchitecture Architecture,
        string PlatformVersion = "");

    /// <summary>
    /// Builds a browser-like desktop user agent that Matomo's device detector can parse
    /// (e.g. "Mozilla/5.0 (Windows NT 10.0; Win64; x64) kDrive/4.0.1").
    /// Mirrors the logic of matomo-sdk-qt's UserAgentBuilder so all kDrive clients report consistently.
    /// </summary>
    internal static class UserAgentBuilder
    {
        private const string DefaultWindowsNtVersion = "10.0";

        public static string BuildDesktopUserAgent(UserAgentInfo info)
        {
            string product = ProductToken(info);
            if (product.Length == 0)
                return string.Empty;

            return $"Mozilla/5.0 ({WindowsPlatformComment(info)}) {product}";
        }

        /// <summary>
        /// Builds the JSON client hints sent as Matomo's "uadata" parameter.
        /// Windows 10 and 11 share the "Windows NT 10.0" user agent; Matomo only tells them apart through
        /// the platformVersion hint (major version above 10 means Windows 11), as browsers report it.
        /// </summary>
        public static string BuildClientHints(UserAgentInfo info)
        {
            string platformVersion = NormalizeVersion(info.PlatformVersion);
            if (platformVersion.Length == 0)
                return string.Empty;

            return JsonSerializer.Serialize(new { platform = "Windows", platformVersion, mobile = false, model = "" });
        }

        public static UserAgentInfo CurrentDesktopInfo(string productName, string productVersion, string platformVersion)
        {
            var osVersion = Environment.OSVersion.Version;
            return new UserAgentInfo(
                productName,
                productVersion,
                $"{osVersion.Major}.{osVersion.Minor}.{osVersion.Build}",
                ArchitectureFrom(RuntimeInformation.OSArchitecture),
                platformVersion);
        }

        public static UserAgentArchitecture ArchitectureFrom(Architecture architecture)
        {
            return architecture switch
            {
                Architecture.X64 => UserAgentArchitecture.X64,
                Architecture.X86 => UserAgentArchitecture.X86,
                Architecture.Arm64 => UserAgentArchitecture.Arm64,
                Architecture.Arm or Architecture.Armv6 => UserAgentArchitecture.Arm,
                _ => UserAgentArchitecture.Unknown
            };
        }

        private static string WindowsPlatformComment(UserAgentInfo info)
        {
            string version = WindowsNtVersion(info.OperatingSystemVersion);
            if (version.Length == 0)
                version = DefaultWindowsNtVersion;

            // Browsers report "Win64; x64" on ARM64 Windows too; Matomo expects this form.
            return info.Architecture switch
            {
                UserAgentArchitecture.X64 or UserAgentArchitecture.Arm64 => $"Windows NT {version}; Win64; x64",
                UserAgentArchitecture.Arm => $"Windows NT {version}; ARM",
                _ => $"Windows NT {version}"
            };
        }

        private static string ProductToken(UserAgentInfo info)
        {
            string name = TokenPart(info.ProductName);
            if (name.Length == 0)
                return string.Empty;

            string version = TokenPart(info.ProductVersion);
            return version.Length == 0 ? name : $"{name}/{version}";
        }

        private static string TokenPart(string? value)
        {
            if (string.IsNullOrWhiteSpace(value))
                return string.Empty;

            var token = new StringBuilder(value.Length);
            bool lastWasSeparator = false;
            foreach (char character in value.Trim())
            {
                bool tokenCharacter = char.IsAsciiLetterOrDigit(character) || character is '.' or '_' or '+';
                bool separator = char.IsWhiteSpace(character) || character == '-';

                if (tokenCharacter)
                {
                    token.Append(character);
                    lastWasSeparator = false;
                }
                else if (separator && token.Length > 0 && !lastWasSeparator)
                {
                    token.Append('-');
                    lastWasSeparator = true;
                }
            }

            return token.ToString().TrimEnd('-');
        }

        private static string WindowsNtVersion(string? operatingSystemVersion)
        {
            var components = NormalizeVersion(operatingSystemVersion).Split('.', StringSplitOptions.RemoveEmptyEntries);
            return string.Join('.', components.Take(2));
        }

        private static string NormalizeVersion(string? version)
        {
            if (string.IsNullOrWhiteSpace(version))
                return string.Empty;

            var normalized = new StringBuilder(version.Length);
            bool lastWasSeparator = false;
            foreach (char character in version.Trim())
            {
                if (char.IsAsciiDigit(character))
                {
                    normalized.Append(character);
                    lastWasSeparator = false;
                }
                else if (character is '.' or '_' && normalized.Length > 0 && !lastWasSeparator)
                {
                    normalized.Append('.');
                    lastWasSeparator = true;
                }
            }

            return normalized.ToString().TrimEnd('.');
        }
    }
}
