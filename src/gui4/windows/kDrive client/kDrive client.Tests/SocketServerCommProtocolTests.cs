using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.ServerCommunication.Services;
using Infomaniak.kDrive.Types;
using System.Net.Sockets;
using System.Reflection;
using System.Text;
using System.Text.Json.Nodes;
using static Infomaniak.kDrive.ServerCommunication.Interfaces.IServerCommProtocol;

namespace Infomaniak.kDrive.Tests;

public class SocketServerCommProtocolTests
{
    private static readonly BindingFlags _instancePrivate = BindingFlags.Instance | BindingFlags.NonPublic;

    [Fact]
    public async Task InitConnection_ReturnsFalse_WhenServerUnavailableAndCancelled()
    {
        string commPath = CreateCommFilePath();
        await File.WriteAllTextAsync(commPath, "65530");
        var protocol = CreateProtocol(commPath);

        using var cts = new CancellationTokenSource(TimeSpan.FromMilliseconds(800));
        bool connected = await protocol.InitConnection(cts.Token);

        Assert.False(connected);
        await ShutdownProtocolAsync(protocol);
    }

    [Fact]
    public async Task InitConnection_EventuallyConnects_WhenServerStartsLater()
    {
        string commPath = CreateCommFilePath();
        var protocol = CreateProtocol(commPath);
        await using var server = new FakeSocketServer();

        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        Task<bool> initTask = protocol.InitConnection(cts.Token);

        await Task.Delay(250);
        await server.WriteCommFileAsync(commPath);

        Assert.True(await initTask);
        await server.WaitForClientAsync();
        await ShutdownProtocolAsync(protocol);
    }

    [Fact]
    public async Task ConnectionLost_Raised_WhenServerClosesConnection()
    {
        string commPath = CreateCommFilePath();
        await using var server = new FakeSocketServer();
        await server.WriteCommFileAsync(commPath);

        var protocol = CreateProtocol(commPath);
        var connectionLost = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        protocol.ConnectionLost += (_, _) => connectionLost.TrySetResult();

        using var initCts = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        Assert.True(await protocol.InitConnection(initCts.Token));
        await server.WaitForClientAsync();

        await server.CloseClientGracefullyAsync();

        await WaitAsync(connectionLost.Task, TimeSpan.FromSeconds(5));
        await ShutdownProtocolAsync(protocol);
    }

    [Fact]
    public async Task SendRequestAsync_ReturnsResponse_ForSuccessfulRoundtrip()
    {
        string commPath = CreateCommFilePath();
        await using var server = new FakeSocketServer();
        await server.WriteCommFileAsync(commPath);

        var protocol = CreateProtocol(commPath);
        using var initCts = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        Assert.True(await protocol.InitConnection(initCts.Token));
        await server.WaitForClientAsync();

        Task replyTask = Task.Run(async () =>
        {
            JsonObject request = await server.ReceiveJsonAsync();
            var response = new JsonObject
            {
                ["type"] = (int)CommMessageType.Request,
                ["id"] = request["id"]!.GetValue<int>(),
                ["num"] = (int)RequestNum.UTILITY_GET_APPSTATE,
                ["params"] = new JsonObject { ["status"] = "ok" }
            };

            await server.SendJsonAsync(response);
        });

        CommData result = await protocol.SendRequestAsync(RequestNum.UTILITY_GET_APPSTATE, new JsonObject());

        await replyTask;
        Assert.Equal(RequestNum.UTILITY_GET_APPSTATE, result.RequestNum);
        Assert.Equal("ok", result.Params["status"]?.GetValue<string>());
        await ShutdownProtocolAsync(protocol);
    }

    [Fact]
    public async Task SendRequestAsync_HandlesHighFrequencyConsecutiveMessages()
    {
        string commPath = CreateCommFilePath();
        await using var server = new FakeSocketServer();
        await server.WriteCommFileAsync(commPath);

        var protocol = CreateProtocol(commPath);
        using var initCts = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        Assert.True(await protocol.InitConnection(initCts.Token));
        await server.WaitForClientAsync();

        const int messageCount = 100;
        Task responder = Task.Run(async () =>
        {
            for (int i = 0; i < messageCount; i++)
            {
                JsonObject request = await server.ReceiveJsonAsync(TimeSpan.FromSeconds(10));
                var response = new JsonObject
                {
                    ["type"] = (int)CommMessageType.Request,
                    ["id"] = request["id"]!.GetValue<int>(),
                    ["num"] = request["num"]!.GetValue<int>(),
                    ["params"] = new JsonObject { ["index"] = i }
                };
                await server.SendJsonAsync(response);
            }
        });

        for (int i = 0; i < messageCount; i++)
        {
            var result = await protocol.SendRequestAsync(RequestNum.UTILITY_GET_APPSTATE, new JsonObject());
            Assert.Equal(i, result.Params["index"]?.GetValue<int>());
        }

        await responder;
        await ShutdownProtocolAsync(protocol);
    }

    [Fact]
    public async Task SignalHandling_ParsesMultipleMessagesInSinglePacket()
    {
        string commPath = CreateCommFilePath();
        await using var server = new FakeSocketServer();
        await server.WriteCommFileAsync(commPath);

        var protocol = CreateProtocol(commPath);
        using var initCts = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        Assert.True(await protocol.InitConnection(initCts.Token));
        await server.WaitForClientAsync();

        var received = new List<SignalNum>();
        var done = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);

        protocol.SignalReceived += (_, args) =>
        {
            received.Add(args.SignalNum);
            if (received.Count == 2)
            {
                done.TrySetResult();
            }
        };

        var signal1 = new JsonObject
        {
            ["type"] = (int)CommMessageType.Signal,
            ["id"] = 1,
            ["num"] = (int)SignalNum.UTILITY_SHOW_SYNTHESIS,
            ["params"] = new JsonObject { ["value"] = "A" }
        };

        var signal2 = new JsonObject
        {
            ["type"] = (int)CommMessageType.Signal,
            ["id"] = 2,
            ["num"] = (int)SignalNum.UTILITY_SHOW_SETTINGS,
            ["params"] = new JsonObject { ["value"] = "B" }
        };

        var payload = Encoding.Unicode.GetBytes(signal1.ToJsonString() + signal2.ToJsonString());
        await server.SendRawAsync(payload);

        await WaitAsync(done.Task, TimeSpan.FromSeconds(5));
        Assert.Equal([SignalNum.UTILITY_SHOW_SYNTHESIS, SignalNum.UTILITY_SHOW_SETTINGS], received);
        await ShutdownProtocolAsync(protocol);
    }

    [Fact]
    public async Task SignalHandling_ParsesFragmentedMessageAcrossArbitraryChunks_WithMultibyteCharacters()
    {
        string commPath = CreateCommFilePath();
        await using var server = new FakeSocketServer();
        await server.WriteCommFileAsync(commPath);

        var protocol = CreateProtocol(commPath);
        using var initCts = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        Assert.True(await protocol.InitConnection(initCts.Token));
        await server.WaitForClientAsync();

        var received = new TaskCompletionSource<string>(TaskCreationOptions.RunContinuationsAsynchronously);
        protocol.SignalReceived += (_, args) =>
        {
            if (args.SignalNum == SignalNum.UTILITY_SHOW_NOTIFICATION)
            {
                received.TrySetResult(args.SignalData["message"]?.GetValue<string>() ?? string.Empty);
            }
        };

        const string expected = "Salut 🙂 漢字 Привет";
        var signal = new JsonObject
        {
            ["type"] = (int)CommMessageType.Signal,
            ["id"] = 99,
            ["num"] = (int)SignalNum.UTILITY_SHOW_NOTIFICATION,
            ["params"] = new JsonObject { ["message"] = expected }
        };

        byte[] bytes = Encoding.Unicode.GetBytes(signal.ToJsonString());
        await server.SendRawAsync(bytes, [3, 5, 2, 7, 1, 13, 9]);

        string actual = await WaitAsync(received.Task, TimeSpan.FromSeconds(5));
        Assert.Equal(expected, actual);
        await ShutdownProtocolAsync(protocol);
    }

    [Fact]
    public async Task SignalHandling_ParsesLargeMessagesBeyondReceiveBuffer()
    {
        string commPath = CreateCommFilePath();
        await using var server = new FakeSocketServer();
        await server.WriteCommFileAsync(commPath);

        var protocol = CreateProtocol(commPath);
        using var initCts = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        Assert.True(await protocol.InitConnection(initCts.Token));
        await server.WaitForClientAsync();

        string largeValue = new('x', 120_000);
        var received = new TaskCompletionSource<int>(TaskCreationOptions.RunContinuationsAsynchronously);
        protocol.SignalReceived += (_, args) =>
        {
            if (args.SignalNum == SignalNum.UTILITY_SHOW_NOTIFICATION)
            {
                received.TrySetResult(args.SignalData["payload"]?.GetValue<string>()?.Length ?? -1);
            }
        };

        var signal = new JsonObject
        {
            ["type"] = (int)CommMessageType.Signal,
            ["id"] = 10,
            ["num"] = (int)SignalNum.UTILITY_SHOW_NOTIFICATION,
            ["params"] = new JsonObject { ["payload"] = largeValue }
        };

        await server.SendJsonAsync(signal, [1024, 2048, 4096, 8192]);

        int length = await WaitAsync(received.Task, TimeSpan.FromSeconds(5));
        Assert.Equal(largeValue.Length, length);
        await ShutdownProtocolAsync(protocol);
    }

    [Fact]
    public async Task ConnectionLost_Raised_WhenPayloadIsMalformed()
    {
        string commPath = CreateCommFilePath();
        await using var server = new FakeSocketServer();
        await server.WriteCommFileAsync(commPath);

        var protocol = CreateProtocol(commPath);
        var lost = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        protocol.ConnectionLost += (_, _) => lost.TrySetResult();

        using var initCts = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        Assert.True(await protocol.InitConnection(initCts.Token));
        await server.WaitForClientAsync();

        await server.SendRawAsync(Encoding.Unicode.GetBytes("INVALID"));

        await WaitAsync(lost.Task, TimeSpan.FromSeconds(5));
        await ShutdownProtocolAsync(protocol);
    }

    [Fact]
    public async Task SendRequestAsync_ReturnsDefault_WhenCancelledWhileWaitingForReply()
    {
        string commPath = CreateCommFilePath();
        await using var server = new FakeSocketServer();
        await server.WriteCommFileAsync(commPath);

        var protocol = CreateProtocol(commPath);
        using var initCts = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        Assert.True(await protocol.InitConnection(initCts.Token));
        await server.WaitForClientAsync();

        Task captureRequest = server.ReceiveJsonAsync();

        using var requestCts = new CancellationTokenSource(TimeSpan.FromMilliseconds(250));
        CommData result = await protocol.SendRequestAsync(RequestNum.UTILITY_GET_APPSTATE, new JsonObject(), requestCts.Token);

        await captureRequest;
        Assert.Equal(-1, result.Num);
        await ShutdownProtocolAsync(protocol);
    }

    [Fact]
    public async Task MultipleInstances_CanCommunicateConcurrently()
    {
        var servers = new List<FakeSocketServer>();
        var protocols = new List<SocketServerCommProtocol>();

        try
        {
            for (int i = 0; i < 3; i++)
            {
                var server = new FakeSocketServer();
                servers.Add(server);

                string commPath = CreateCommFilePath();
                await server.WriteCommFileAsync(commPath);
                var protocol = CreateProtocol(commPath);
                protocols.Add(protocol);

                using var initCts = new CancellationTokenSource(TimeSpan.FromSeconds(5));
                Assert.True(await protocol.InitConnection(initCts.Token));
                await server.WaitForClientAsync();
            }

            var tasks = protocols.Select((protocol, index) => Task.Run(async () =>
            {
                var requestTask = servers[index].ReceiveJsonAsync();
                var responseTask = Task.Run(async () =>
                {
                    JsonObject request = await requestTask;
                    var response = new JsonObject
                    {
                        ["type"] = (int)CommMessageType.Request,
                        ["id"] = request["id"]!.GetValue<int>(),
                        ["num"] = request["num"]!.GetValue<int>(),
                        ["params"] = new JsonObject { ["instance"] = index }
                    };
                    await servers[index].SendJsonAsync(response);
                });

                var result = await protocol.SendRequestAsync(RequestNum.UTILITY_GET_APPSTATE, new JsonObject());
                await responseTask;
                return result.Params["instance"]?.GetValue<int>();
            }));

            int?[] values = await Task.WhenAll(tasks);
            Assert.Equal([0, 1, 2], values);
        }
        finally
        {
            foreach (var protocol in protocols)
            {
                await ShutdownProtocolAsync(protocol);
            }

            foreach (var server in servers)
            {
                await server.DisposeAsync();
            }
        }
    }

    [Fact]
    public async Task RapidConnectDisconnectCycles_RemainStable()
    {
        for (int i = 0; i < 10; i++)
        {
            string commPath = CreateCommFilePath();
            await using var server = new FakeSocketServer();
            await server.WriteCommFileAsync(commPath);
            var protocol = CreateProtocol(commPath);

            var lost = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
            protocol.ConnectionLost += (_, _) => lost.TrySetResult();

            using var initCts = new CancellationTokenSource(TimeSpan.FromSeconds(5));
            Assert.True(await protocol.InitConnection(initCts.Token));
            await server.WaitForClientAsync();

            await server.CrashClientConnectionAsync();
            await WaitAsync(lost.Task, TimeSpan.FromSeconds(5));
            await ShutdownProtocolAsync(protocol);
        }
    }

    private static SocketServerCommProtocol CreateProtocol(string commPath)
    {
        var protocol = new SocketServerCommProtocol();
        var field = typeof(SocketServerCommProtocol).GetField("_commPortFilePath", _instancePrivate)
                    ?? throw new InvalidOperationException("Failed to find _commPortFilePath field.");
        field.SetValue(protocol, commPath);
        return protocol;
    }

    private static string CreateCommFilePath()
    {
        string dir = Path.Combine(Path.GetTempPath(), "kdrive-socket-tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        return Path.Combine(dir, ".comm");
    }

    private static async Task ShutdownProtocolAsync(SocketServerCommProtocol protocol)
    {
        var stopField = typeof(SocketServerCommProtocol).GetField("_stopRequested", _instancePrivate)
                        ?? throw new InvalidOperationException("Failed to find _stopRequested field.");
        stopField.SetValue(protocol, true);

        var socketField = typeof(SocketServerCommProtocol).GetField("_socket", _instancePrivate)
                          ?? throw new InvalidOperationException("Failed to find _socket field.");
        (socketField.GetValue(protocol) as Socket)?.Dispose();

        var pollingTaskField = typeof(SocketServerCommProtocol).GetField("_pollingTask", _instancePrivate)
                               ?? throw new InvalidOperationException("Failed to find _pollingTask field.");

        if (pollingTaskField.GetValue(protocol) is Task pollingTask)
        {
            await Task.WhenAny(pollingTask, Task.Delay(TimeSpan.FromSeconds(2)));
        }
    }

    private static async Task WaitAsync(Task task, TimeSpan timeout)
    {
        var completed = await Task.WhenAny(task, Task.Delay(timeout));
        Assert.True(completed == task, "Operation timed out.");
        await task;
    }

    private static async Task<T> WaitAsync<T>(Task<T> task, TimeSpan timeout)
    {
        var completed = await Task.WhenAny(task, Task.Delay(timeout));
        Assert.True(completed == task, "Operation timed out.");
        return await task;
    }
}
