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
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Infomaniak.kDrive
{
    public class BinaryReader : System.IO.BinaryReader
    {
        public BinaryReader(System.IO.Stream input) : base(input)
        {
        }
        override public string ReadString()
        {
            var length = base.ReadInt32();
            var bytes = base.ReadBytes(length);
            return Encoding.UTF8.GetString(bytes).Replace("\0", "");
        }

        public Image? ReadPNG()
        {
            base.ReadInt32(); // skip length

            var originPosition = base.BaseStream.Position;
            MemoryStream ms = new MemoryStream();
            base.BaseStream.CopyTo(ms);

            byte[] pngBytes = ms.ToArray();

            // Find the PNG end signature (IEND chunk)
            int iendIndex = -1;
            for (int i = 0; i < pngBytes.Length - 8; i++)
            {
                if (pngBytes[i] == 0x49 && pngBytes[i + 1] == 0x45 && pngBytes[i + 2] == 0x4E && pngBytes[i + 3] == 0x44 &&
                    pngBytes[i + 4] == 0xAE && pngBytes[i + 5] == 0x42 && pngBytes[i + 6] == 0x60 && pngBytes[i + 7] == 0x82)
                {
                    iendIndex = i + 8; // Include the length of the IEND chunk
                    break;
                }
            }
            if (iendIndex == -1)
            {
                // IEND chunk not found, return null or handle the error as needed
                base.BaseStream.Position = originPosition;
                return null;
            }
            // Adjust the stream position to just after the PNG data
            base.BaseStream.Position = originPosition + iendIndex;
            pngBytes = pngBytes.Take(iendIndex).ToArray();

            return Image.FromStream(new MemoryStream(pngBytes));
        }
    }
}
