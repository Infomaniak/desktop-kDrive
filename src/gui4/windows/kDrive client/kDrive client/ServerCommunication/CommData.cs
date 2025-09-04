/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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
using System.Collections.Immutable;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace KDrive.ServerCommunication
{
    public class CommData
    {
        // The name of member variables need to be the same as the JSON keys, it should be keeped as is even if it breaks C# naming conventions.
        public int type { get; set; } = -1; // 0=request, 1=reply, 2=signal
        public int id { get; set; } = -1;
        public int num { get; set; } = -1;
        public string? @params { get; set; }
        public string? result { get; set; }


        public ImmutableArray<byte> ParmsByteArray
        {
            get
            {
                if (string.IsNullOrEmpty(@params))
                {
                    return ImmutableArray<byte>.Empty;
                }
                try
                {
                    byte[] bytes = Convert.FromBase64String(@params);
                    return ImmutableArray.Create(bytes);
                }
                catch (FormatException)
                {
                    Logger.Log(Logger.Level.Warning, "Invalid base64 string in params.");
                    return ImmutableArray<byte>.Empty;
                }
            }
        }

        public ImmutableArray<byte> ResultByteArray
        {
            get
            {
                if (string.IsNullOrEmpty(result))
                {
                    return ImmutableArray<byte>.Empty;
                }
                try
                {
                    byte[] bytes = Convert.FromBase64String(result);
                    return ImmutableArray.Create(bytes);
                }
                catch (FormatException)
                {
                    Logger.Log(Logger.Level.Warning, "Invalid base64 string in result.");
                    return ImmutableArray<byte>.Empty;
                }
            }
        }
    }
}
