using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace kDrive_client.ServerCommunication
{
    internal class CommData
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
                    Logger.LogWarning("Invalid base64 string in params.");
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
                    Logger.LogWarning("Invalid base64 string in result.");
                    return ImmutableArray<byte>.Empty;
                }
            }
        }
    }
}
