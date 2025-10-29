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
    internal class ColorJsonConverter : JsonConverter<System.Drawing.Color>
    {
        public override System.Drawing.Color Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
        {
            // Decode base64 strings
            if (reader.TokenType != JsonTokenType.String)
                throw new JsonException("Expected string token type for ColorJsonConverter.");
            string colorString = "";

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

