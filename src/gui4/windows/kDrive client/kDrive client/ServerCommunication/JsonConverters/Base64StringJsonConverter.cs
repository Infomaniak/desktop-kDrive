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
    internal class Base64StringJsonConverter : JsonConverter<string>
    {
        public override string Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
        {
            // Decode base64 strings
            if (reader.TokenType != JsonTokenType.String)
                throw new JsonException("Expected string token type for Base64StringJsonConverter.");

            try
            {
                byte[] bytes = reader.GetBytesFromBase64();
                return Encoding.UTF8.GetString(bytes);
            }
            catch (FormatException ex)
            {
                return reader.GetString() ?? throw new JsonException("Invalid Base64 string format.", ex);
            }
        }
        public override void Write(Utf8JsonWriter writer, string value, JsonSerializerOptions options)
        {
            // Encode strings to base64
            byte[] bytes = Encoding.UTF8.GetBytes(value);
            writer.WriteBase64StringValue(bytes);
        }
    }
}

