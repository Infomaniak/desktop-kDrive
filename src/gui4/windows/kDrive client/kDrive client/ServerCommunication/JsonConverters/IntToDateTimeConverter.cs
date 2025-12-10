using System;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Infomaniak.kDrive.ServerCommunication.JsonConverters
{
    internal class IntToDateTimeConverter : JsonConverter<DateTime>
    {
        public override DateTime Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
        {
            // Expecting an integer Unix timestamp (seconds since epoch)
            long ms = reader.GetInt64();
            return DateTimeOffset.FromUnixTimeSeconds(ms).LocalDateTime;
        }

        public override void Write(Utf8JsonWriter writer, DateTime value, JsonSerializerOptions options)
        {
            long ms = new DateTimeOffset(value).ToUnixTimeSeconds();
            writer.WriteNumberValue(ms);
        }
    }
}
