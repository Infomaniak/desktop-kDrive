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

        [System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "IDE1006:Naming Styles", Justification = "<Pending>")]
        public class CommData
        {
            public CommMessageType Type { get; set; } = CommMessageType.Unknown;
            public int Id { get; set; } = -1;
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

            public JsonObject? Params { get; set; }
            public string? Result { get; set; }
        }

        Task<CommData> SendRequestAsync(RequestNum requestNum, JsonObject parameters, CancellationToken cancellationToken = default);

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
