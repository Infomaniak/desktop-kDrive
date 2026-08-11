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

using Infomaniak.kDrive.ServerCommunication.Interfaces;
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Infomaniak.kDrive.ServerCommunication.Services
{
    /// <summary>
    /// Reads secrets from the Windows Credential Manager, the store the kDrive server writes to.
    /// The target name and blob encoding mirror the server-side keychain implementation
    /// (<c>package.service/user</c>, UTF-8 blob), so the client reads back exactly what the
    /// server stored.
    /// </summary>
    public sealed class WindowsKeychainStore : IKeychainStore
    {
        // Must match the server-side keychain identifiers (see keychainstorage.cpp).
        private const string Package = "com.infomaniak.drive";
        private const string Service = "desktopclient";

        private const int CRED_TYPE_GENERIC = 1;

        public string? ReadSecret(string key)
        {
            string targetName = $"{Package}.{Service}/{key}";

            if (!CredRead(targetName, CRED_TYPE_GENERIC, 0, out IntPtr credentialPtr))
            {
                int error = Marshal.GetLastWin32Error();
                Logger.Log(Logger.Level.Warning, $"Failed to read keychain entry '{targetName}': Win32 error {error}.");
                return null;
            }

            try
            {
                var credential = Marshal.PtrToStructure<CREDENTIAL>(credentialPtr);
                if (credential.CredentialBlob == IntPtr.Zero || credential.CredentialBlobSize == 0)
                {
                    return null;
                }

                var blob = new byte[credential.CredentialBlobSize];
                Marshal.Copy(credential.CredentialBlob, blob, 0, (int)credential.CredentialBlobSize);
                return Encoding.UTF8.GetString(blob);
            }
            finally
            {
                CredFree(credentialPtr);
            }
        }

        [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool CredRead(string target, int type, int reservedFlag, out IntPtr credentialPtr);

        [DllImport("advapi32.dll")]
        private static extern void CredFree(IntPtr buffer);

        [StructLayout(LayoutKind.Sequential)]
        private struct CREDENTIAL
        {
            public int Flags;
            public int Type;
            public IntPtr TargetName;
            public IntPtr Comment;
            public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten;
            public uint CredentialBlobSize;
            public IntPtr CredentialBlob;
            public int Persist;
            public int AttributeCount;
            public IntPtr Attributes;
            public IntPtr TargetAlias;
            public IntPtr UserName;
        }
    }
}
