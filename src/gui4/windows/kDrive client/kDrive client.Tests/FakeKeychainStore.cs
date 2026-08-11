using Infomaniak.kDrive.ServerCommunication.Interfaces;

namespace Infomaniak.kDrive.Tests;

/// <summary>
/// Test double for <see cref="IKeychainStore"/> that always returns a fixed PEM value, used to
/// simulate a pinned certificate that does not match the one the server actually presents.
/// </summary>
internal sealed class StaticPemKeychainStore(string pem) : IKeychainStore
{
    public string? ReadSecret(string key) => pem;
}

/// <summary>
/// Test double for <see cref="IKeychainStore"/> backed by an in-memory map, used to publish the
/// TLS material a <see cref="FakeSocketServer"/> presents without going through companion files.
/// Mirrors the real deployment where the server publishes the server certificate, the client
/// certificate and the client private key under distinct keychain keys.
/// </summary>
internal sealed class InMemoryKeychainStore : IKeychainStore
{
    // Must match the keychain keys used by TcpServerCommClient (see comm.h).
    private const string ServerCertKey = "kdrive_ipc_tls_cert";
    private const string ClientCertKey = "kdrive_ipc_tls_client_cert";
    private const string ClientPrivateKeyKey = "kdrive_ipc_tls_client_key";

    private readonly Dictionary<string, string> _secrets;

    public InMemoryKeychainStore(string serverCertPem, string clientCertPem, string clientKeyPem)
    {
        _secrets = new Dictionary<string, string>
        {
            [ServerCertKey] = serverCertPem,
            [ClientCertKey] = clientCertPem,
            [ClientPrivateKeyKey] = clientKeyPem
        };
    }

    public string? ReadSecret(string key) => _secrets.TryGetValue(key, out var value) ? value : null;
}
