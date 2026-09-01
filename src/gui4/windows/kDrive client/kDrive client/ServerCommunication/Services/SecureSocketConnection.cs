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
using System.Linq;
using System.Net.Security;
using System.Net.Sockets;
using System.Security.Authentication;
using System.Security.Cryptography.X509Certificates;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ServerCommunication.Services
{
    /// <summary>
    /// Establishes a mutually-authenticated TLS connection to the local kDrive server socket.
    /// The server exposes a <c>SecureServerSocket</c> secured with an auto-generated,
    /// self-signed certificate over the loopback interface. The client pins that exact
    /// certificate (retrieved from the OS keychain) and rejects any other one. In turn the
    /// server requires the client to present the client certificate it published in the
    /// keychain, so the client authenticates itself during the same handshake.
    /// </summary>
    internal static class SecureSocketConnection
    {
        /// <summary>
        /// Opens a TCP connection to <paramref name="host"/>:<paramref name="port"/> and performs
        /// the TLS client handshake, validating the server against the <paramref name="serverCertificate"/>
        /// and presenting <paramref name="clientCertificate"/> so the server can authenticate the client.
        /// </summary>
        /// <returns>
        /// The authenticated <see cref="SslStream"/> used to read and write application data.
        /// </returns>
        public static async Task<SslStream> ConnectAsync(string host, int port, X509Certificate2 serverCertificate, X509Certificate2 clientCertificate, CancellationToken cancellationToken)
        {
            var socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
            try
            {
                await socket.ConnectAsync(host, port, cancellationToken).ConfigureAwait(false);

                var networkStream = new NetworkStream(socket, ownsSocket: true);
                var sslStream = new SslStream(networkStream, leaveInnerStreamOpen: false);

                var options = new SslClientAuthenticationOptions
                {
                    TargetHost = host,
                    EnabledSslProtocols = SslProtocols.Tls12 | SslProtocols.Tls13,
                    ClientCertificates = new X509CertificateCollection { clientCertificate },
                    RemoteCertificateValidationCallback =
                        (sender, certificate, chain, errors) => ValidateServerCertificate(certificate, serverCertificate)
                };

                await sslStream.AuthenticateAsClientAsync(options, cancellationToken).ConfigureAwait(false);
                return sslStream;
            }
            catch
            {
                socket.Dispose();
                throw;
            }
        }

        /// <summary>
        /// Builds an <see cref="X509Certificate2"/> that carries its private key from the PEM-encoded
        /// certificate and private key the server published in the keychain, so the client can present
        /// it during the TLS handshake.
        /// </summary>
        /// <remarks>
        /// On Windows, SChannel cannot use the private key produced directly by
        /// <see cref="X509Certificate2.CreateFromPem(System.ReadOnlySpan{char}, System.ReadOnlySpan{char})"/>
        /// during a TLS handshake. Round-tripping through a PKCS#12 blob yields a certificate whose private
        /// key SChannel can access, mirroring the workaround used elsewhere for the same limitation. The
        /// key set is not persisted, so the temporary key container is removed when the returned
        /// certificate is disposed.
        /// </remarks>
        /// <returns>The certificate with its private key, or <c>null</c> when the PEM material is missing or invalid.</returns>
        internal static X509Certificate2? BuildClientCertificate(string? certificatePem, string? privateKeyPem)
        {
            if (string.IsNullOrEmpty(certificatePem) || string.IsNullOrEmpty(privateKeyPem))
            {
                return null;
            }

            try
            {
                using var certificateWithKey = X509Certificate2.CreateFromPem(certificatePem, privateKeyPem);
                byte[] pkcs12 = certificateWithKey.Export(X509ContentType.Pkcs12);
                return X509CertificateLoader.LoadPkcs12(pkcs12, null, X509KeyStorageFlags.Exportable);
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Failed to build TLS client certificate from keychain material: {ex.Message}");
                return null;
            }
        }

        /// <summary>
        /// Validates the certificate presented by the server against the server certificate.
        /// Since the certificate is self-signed there is no CA chain to trust; instead we require
        /// the presented certificate to be byte-for-byte identical to the one the server published
        /// in the keychain.
        /// </summary>
        private static bool ValidateServerCertificate(X509Certificate? presentedCertificate, X509Certificate2 serverCertificate)
        {
            if (presentedCertificate is null)
            {
                Logger.Log(Logger.Level.Error, "TLS validation failed: server presented no certificate.");
                return false;
            }

            using var presented = new X509Certificate2(presentedCertificate);
            bool matches = presented.RawData.SequenceEqual(serverCertificate.RawData);
            if (!matches)
            {
                Logger.Log(Logger.Level.Error, "TLS validation failed: server certificate does not match the server certificate.");
            }
            return matches;
        }
    }
}
