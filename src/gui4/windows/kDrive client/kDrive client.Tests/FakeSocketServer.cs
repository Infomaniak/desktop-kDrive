using System.Net;
using System.Net.Security;
using System.Net.Sockets;
using System.Security.Authentication;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Text.Json.Nodes;

namespace Infomaniak.kDrive.Tests;

internal sealed class FakeSocketServer : IAsyncDisposable
{
    private readonly TcpListener _listener;
    private readonly CancellationTokenSource _acceptCts = new();
    private readonly Task<Socket> _acceptTask;
    private readonly X509Certificate2 _serverCertificate;
    private Socket? _client;
    private SslStream? _clientStream;

    public int Port { get; }

    /// <summary>
    /// PEM-encoded certificate the fake server presents. Tests feed this to the client's
    /// keychain store so the client can pin/validate it, mirroring the real deployment.
    /// </summary>
    public string CertificatePem => _serverCertificate.ExportCertificatePem();

    public FakeSocketServer()
    {
        _serverCertificate = CreateSelfSignedCertificate();
        _listener = new TcpListener(IPAddress.Loopback, 0);
        _listener.Start();
        Port = ((IPEndPoint)_listener.LocalEndpoint).Port;
        _acceptTask = _listener.AcceptSocketAsync(_acceptCts.Token).AsTask();
    }

    public async Task WriteCommFileAsync(string commFilePath)
    {
        var dir = Path.GetDirectoryName(commFilePath);
        if (!string.IsNullOrEmpty(dir))
        {
            Directory.CreateDirectory(dir);
        }

        // Publish the certificate alongside the port, emulating the server publishing it to the
        // keychain. FakeKeychainStore reads it back so the client can pin/validate it.
        await File.WriteAllTextAsync(CertificateFilePath(commFilePath), CertificatePem);
        await File.WriteAllTextAsync(commFilePath, Port.ToString());
    }

    /// <summary>
    /// Convention shared with <c>FakeKeychainStore</c> for the companion file holding the
    /// server certificate associated with a given .comm file.
    /// </summary>
    public static string CertificateFilePath(string commFilePath) => commFilePath + ".cert";

    public async Task<Socket> WaitForClientAsync(TimeSpan? timeout = null)
    {
        timeout ??= TimeSpan.FromSeconds(5);
        var completed = await Task.WhenAny(_acceptTask, Task.Delay(timeout.Value));
        if (completed != _acceptTask)
        {
            throw new TimeoutException("Timed out waiting for client connection.");
        }

        _client = await _acceptTask;

        var networkStream = new NetworkStream(_client, ownsSocket: false);
        _clientStream = new SslStream(networkStream, leaveInnerStreamOpen: false);
        await _clientStream.AuthenticateAsServerAsync(new SslServerAuthenticationOptions
        {
            ServerCertificate = _serverCertificate,
            EnabledSslProtocols = SslProtocols.Tls12 | SslProtocols.Tls13,
            ClientCertificateRequired = false
        });

        return _client;
    }

    public async Task<JsonObject> ReceiveJsonAsync(TimeSpan? timeout = null)
    {
        timeout ??= TimeSpan.FromSeconds(5);
        await EnsureClientAsync();
        var stream = _clientStream!;

        using var cts = new CancellationTokenSource(timeout.Value);
        var decoder = Encoding.Unicode.GetDecoder();
        var buffer = new byte[2048];
        var chars = new char[4096];
        var sb = new StringBuilder();

        while (!cts.IsCancellationRequested)
        {
            var bytesRead = await stream.ReadAsync(buffer, cts.Token);
            if (bytesRead == 0)
            {
                throw new IOException("Connection closed while waiting for message.");
            }

            int charCount = decoder.GetChars(buffer, 0, bytesRead, chars, 0, flush: false);
            sb.Append(chars, 0, charCount);

            if (TryExtractFirstJson(sb.ToString(), out var json))
            {
                return json;
            }
        }

        throw new TimeoutException("Timed out receiving JSON message.");
    }

    public async Task SendJsonAsync(JsonObject json, IReadOnlyList<int>? chunkSizes = null)
    {
        string payload = json.ToJsonString();
        await SendRawAsync(Encoding.Unicode.GetBytes(payload), chunkSizes);
    }

    public async Task SendRawAsync(byte[] data, IReadOnlyList<int>? chunkSizes = null)
    {
        await EnsureClientAsync();
        var stream = _clientStream!;

        if (chunkSizes is null || chunkSizes.Count == 0)
        {
            await stream.WriteAsync(data);
            await stream.FlushAsync();
            return;
        }

        int offset = 0;
        foreach (int requested in chunkSizes)
        {
            if (offset >= data.Length)
            {
                break;
            }

            int len = Math.Min(requested, data.Length - offset);
            if (len <= 0)
            {
                continue;
            }

            await stream.WriteAsync(data.AsMemory(offset, len));
            await stream.FlushAsync();
            offset += len;
            await Task.Delay(5);
        }

        if (offset < data.Length)
        {
            await stream.WriteAsync(data.AsMemory(offset));
            await stream.FlushAsync();
        }
    }

    public async Task CloseClientGracefullyAsync()
    {
        var socket = await EnsureClientAsync();
        try
        {
            _clientStream?.Dispose();
            socket.Shutdown(SocketShutdown.Both);
        }
        catch
        {
            // ignore cleanup races
        }
        finally
        {
            socket.Dispose();
            _client = null;
            _clientStream = null;
        }
    }

    public async Task CrashClientConnectionAsync()
    {
        var socket = await EnsureClientAsync();
        _clientStream?.Dispose();
        socket.Dispose();
        _client = null;
        _clientStream = null;
    }

    private async Task<Socket> EnsureClientAsync()
    {
        return _client ?? await WaitForClientAsync();
    }

    private static X509Certificate2 CreateSelfSignedCertificate()
    {
        using var rsa = RSA.Create(2048);
        var request = new CertificateRequest("CN=kDrive-test", rsa, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
        return request.CreateSelfSigned(DateTimeOffset.UtcNow.AddDays(-1), DateTimeOffset.UtcNow.AddDays(1));
    }

    private static bool TryExtractFirstJson(string text, out JsonObject json)
    {
        json = new JsonObject();
        int balance = 0;
        int start = -1;

        for (int i = 0; i < text.Length; i++)
        {
            if (text[i] == '{')
            {
                if (start == -1)
                {
                    start = i;
                }
                balance++;
            }
            else if (text[i] == '}')
            {
                balance--;
                if (balance == 0 && start >= 0)
                {
                    var slice = text[start..(i + 1)];
                    var node = JsonNode.Parse(slice) as JsonObject;
                    if (node is null)
                    {
                        return false;
                    }

                    json = node;
                    return true;
                }
            }
        }

        return false;
    }

    public async ValueTask DisposeAsync()
    {
        _acceptCts.Cancel();
        _listener.Stop();

        if (_clientStream is not null)
        {
            _clientStream.Dispose();
            _clientStream = null;
        }

        if (_client is not null)
        {
            _client.Dispose();
            _client = null;
        }

        try
        {
            await _acceptTask;
        }
        catch
        {
            // expected when canceled/disposed
        }

        _acceptCts.Dispose();
        _serverCertificate.Dispose();
    }
}
