using DynamicData.Aggregation;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Tasks;

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

