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
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Infomaniak.kDrive.ServerCommunication.JsonConverters
{
    internal class ColorJsonConverter : JsonConverter<System.Drawing.Color>
    {
        public override System.Drawing.Color Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
        {
            // Decode base64 strings
            if (reader.TokenType != JsonTokenType.String)
                throw new JsonException("Expected string token type for ColorJsonConverter.");
            string colorString;
            try
            {
                byte[] bytes = reader.GetBytesFromBase64();
                colorString = Encoding.UTF8.GetString(bytes);
            }
            catch (FormatException ex)
            {
                colorString = reader.GetString() ?? throw new JsonException("Invalid Base64 string format.", ex);
            }
            return System.Drawing.ColorTranslator.FromHtml(colorString);
        }
        public override void Write(Utf8JsonWriter writer, System.Drawing.Color value, JsonSerializerOptions options)
        {
            // Encode strings to base64
            string colorString = System.Drawing.ColorTranslator.ToHtml(value);
            byte[] bytes = Encoding.UTF8.GetBytes(colorString);
            writer.WriteBase64StringValue(bytes);
        }
    }
}

