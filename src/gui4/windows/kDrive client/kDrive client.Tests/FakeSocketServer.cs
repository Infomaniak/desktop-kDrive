using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.Json.Nodes;

namespace Infomaniak.kDrive.Tests;

internal sealed class FakeSocketServer : IAsyncDisposable
{
    private readonly TcpListener _listener;
    private readonly CancellationTokenSource _acceptCts = new();
    private readonly Task<Socket> _acceptTask;
    private Socket? _client;

    public int Port { get; }

    public FakeSocketServer()
    {
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

        await File.WriteAllTextAsync(commFilePath, Port.ToString());
    }

    public async Task<Socket> WaitForClientAsync(TimeSpan? timeout = null)
    {
        timeout ??= TimeSpan.FromSeconds(5);
        var completed = await Task.WhenAny(_acceptTask, Task.Delay(timeout.Value));
        if (completed != _acceptTask)
        {
            throw new TimeoutException("Timed out waiting for client connection.");
        }

        _client = await _acceptTask;
        return _client;
    }

    public async Task<JsonObject> ReceiveJsonAsync(TimeSpan? timeout = null)
    {
        timeout ??= TimeSpan.FromSeconds(5);
        var socket = await EnsureClientAsync();

        using var cts = new CancellationTokenSource(timeout.Value);
        var decoder = Encoding.Unicode.GetDecoder();
        var buffer = new byte[2048];
        var chars = new char[4096];
        var sb = new StringBuilder();

        while (!cts.IsCancellationRequested)
        {
            var bytesRead = await socket.ReceiveAsync(buffer, SocketFlags.None, cts.Token);
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
        var socket = await EnsureClientAsync();

        if (chunkSizes is null || chunkSizes.Count == 0)
        {
            await socket.SendAsync(data, SocketFlags.None);
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

            await socket.SendAsync(data.AsMemory(offset, len), SocketFlags.None);
            offset += len;
            await Task.Delay(5);
        }

        if (offset < data.Length)
        {
            await socket.SendAsync(data.AsMemory(offset), SocketFlags.None);
        }
    }

    public async Task CloseClientGracefullyAsync()
    {
        var socket = await EnsureClientAsync();
        try
        {
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
        }
    }

    public async Task CrashClientConnectionAsync()
    {
        var socket = await EnsureClientAsync();
        socket.Dispose();
        _client = null;
    }

    private async Task<Socket> EnsureClientAsync()
    {
        return _client ?? await WaitForClientAsync();
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
    }
}
