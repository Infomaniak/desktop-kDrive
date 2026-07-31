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

using System.Net.Security;
using System.Net.Sockets;
using System.Security.Authentication;
using System.Security.Cryptography.X509Certificates;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ServerCommunication.Services
{
    /// <summary>
    /// Establishes a TLS connection to the local kDrive server socket.
    /// The server exposes a <c>SecureServerSocket</c> secured with an auto-generated,
    /// self-signed certificate over the loopback interface.
    /// </summary>
    internal static class SecureSocketConnection
    {
        /// <summary>
        /// Opens a TCP connection to <paramref name="host"/>:<paramref name="port"/> and performs
        /// the TLS client handshake.
        /// </summary>
        /// <returns>
        /// The connected <see cref="Socket"/> (for polling/availability checks) and the
        /// authenticated <see cref="SslStream"/> used to read and write application data.
        /// </returns>
        public static async Task<(Socket Socket, SslStream Stream)> ConnectAsync(string host, int port, CancellationToken cancellationToken)
        {
            var socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
            try
            {
                await socket.ConnectAsync(host, port, cancellationToken).ConfigureAwait(false);

                var networkStream = new NetworkStream(socket, ownsSocket: true);
                var sslStream = new SslStream(networkStream, leaveInnerStreamOpen: false, ValidateServerCertificate);

                var options = new SslClientAuthenticationOptions
                {
                    TargetHost = host,
                    EnabledSslProtocols = SslProtocols.Tls12 | SslProtocols.Tls13,
                    RemoteCertificateValidationCallback = ValidateServerCertificate
                };

                await sslStream.AuthenticateAsClientAsync(options, cancellationToken).ConfigureAwait(false);
                return (socket, sslStream);
            }
            catch
            {
                socket.Dispose();
                throw;
            }
        }

        private static bool ValidateServerCertificate(object sender, X509Certificate? certificate, X509Chain? chain, SslPolicyErrors sslPolicyErrors)
        {
            // Loopback-only IPC secured by an auto-generated self-signed certificate.
            // There is no CA to validate against, so any presented certificate is accepted.
            return true;
        }
    }
}
