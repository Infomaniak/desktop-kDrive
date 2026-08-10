using Infomaniak.kDrive.ServerCommunication.Interfaces;

namespace Infomaniak.kDrive.Tests;

/// <summary>
/// Test double for <see cref="IKeychainStore"/> that reads the TLS material published by
/// <see cref="FakeSocketServer.WriteCommFileAsync"/> from the companion files, resolving each
/// keychain key to the matching companion file. This mirrors the real deployment where the server
/// publishes the server certificate, the client certificate and the client private key under
/// distinct keychain keys.
/// </summary>
internal sealed class FakeKeychainStore(string commFilePath) : IKeychainStore
{
    // Must match the keychain keys used by TcpServerCommClient (see comm.h).
    private const string ServerCertKey = "kdrive_ipc_tls_cert";
    private const string ClientCertKey = "kdrive_ipc_tls_client_cert";
    private const string ClientKeyKey = "kdrive_ipc_tls_client_key";

    private readonly string _serverCertFilePath = FakeSocketServer.CertificateFilePath(commFilePath);
    private readonly string _clientCertFilePath = FakeSocketServer.ClientCertificateFilePath(commFilePath);
    private readonly string _clientKeyFilePath = FakeSocketServer.ClientKeyFilePath(commFilePath);

    public string? ReadSecret(string key)
    {
        string? path = key switch
        {
            ServerCertKey => _serverCertFilePath,
            ClientCertKey => _clientCertFilePath,
            ClientKeyKey => _clientKeyFilePath,
            _ => null
        };

        return path is not null && File.Exists(path) ? File.ReadAllText(path) : null;
    }
}

/// <summary>
/// Test double for <see cref="IKeychainStore"/> that always returns a fixed PEM value, used to
/// simulate a pinned certificate that does not match the one the server actually presents.
/// </summary>
internal sealed class StaticPemKeychainStore(string pem) : IKeychainStore
{
    public string? ReadSecret(string key) => pem;
}
