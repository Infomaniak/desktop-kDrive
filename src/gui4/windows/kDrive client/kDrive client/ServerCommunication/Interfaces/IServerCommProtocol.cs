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
using Infomaniak.kDrive.Types;
using System;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ServerCommunication.Interfaces
{
    public interface IServerCommProtocol
    {
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

            public ExitCode Code { get; set; } = ExitCode.Unknown;
            public ExitCause Cause { get; set; } = ExitCause.Unknown;

            // Parameters for requests or data for signals
            public JsonObject Params { get; set; } = [];
        }
        public Task<bool> InitConnection(CancellationToken cancellationToken);

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
        event EventHandler ConnectionLost;
    }
}
