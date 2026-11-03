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
using Windows.Foundation.Metadata;

namespace Infomaniak.kDrive.Analytics
{
    /// <summary>
    /// Provides the Windows "Sec-CH-UA-Platform-Version" client hint value, computed like Chromium does:
    /// the major version of the installed UniversalApiContract (10 and below on Windows 10, 13+ on Windows 11).
    /// </summary>
    internal static class WindowsPlatformVersion
    {
        private const string UniversalApiContract = "Windows.Foundation.UniversalApiContract";

        public static string Current()
        {
            try
            {
                ushort major = 0;
                while (major < ushort.MaxValue && ApiInformation.IsApiContractPresent(UniversalApiContract, (ushort)(major + 1)))
                    major++;

                return major == 0 ? string.Empty : $"{major}.0.0";
            }
            catch (Exception ex)
            {
                Logger.LogWarning($"Unable to retrieve the Windows platform version: {ex.Message}",
                    "WindowsPlatformVersion: Unable to retrieve the platform version");
                return string.Empty;
            }
        }
    }
}
