using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using static Infomaniak.kDrive.ServerCommunication.CommShared;

namespace Infomaniak.kDrive.ServerCommunication.Interfaces
{
    public interface IServerCommProtocol
    {
        Task Initialize();

        public class CommData
        {
            // Type can be Request or Signal
            public CommMessageType Type { get; set; } = CommMessageType.Unknown;

            // Unique identifier for the request/response pair or signal
            public int Id { get; set; } = -1;

            // Number identifying the specific request or signal type
            public int Num { get; set; } = -1;
            public SignalNum SignalNum
            {
                get { return (SignalNum)Num; }
                set { Num = (int)value; }
            }
            public RequestNum RequestNum
            {
                get { return (RequestNum)Num; }
                set { Num = (int)value; }
            }

            // Parameters for requests or data for signals
            public JsonObject? Params { get; set; }
        }

        public Task<CommData> SendRequestAsync(RequestNum requestNum, JsonObject parameters, CancellationToken cancellationToken = default);

        // Signals
        public class SignalEventArgs : EventArgs
        {
            public SignalNum SignalNum { get; }
            public JsonObject SignalData { get; }

            public SignalEventArgs(SignalNum signalNum, JsonObject data)
            {
                SignalNum = signalNum;
                SignalData = data;
            }
        }

        event EventHandler<SignalEventArgs> SignalReceived;
    }
}
