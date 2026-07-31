using Infomaniak.kDrive.ServerCommunication.Interfaces;

namespace Infomaniak.kDrive.Tests;

/// <summary>
/// Test double for <see cref="IKeychainStore"/> that reads the server certificate from the
/// companion file written by <see cref="FakeSocketServer.WriteCommFileAsync"/>.
/// </summary>
internal sealed class FakeKeychainStore(string commFilePath) : IKeychainStore
{
    private readonly string _certificateFilePath = FakeSocketServer.CertificateFilePath(commFilePath);

    public string? ReadSecret(string key)
    {
        return File.Exists(_certificateFilePath) ? File.ReadAllText(_certificateFilePath) : null;
    }
}
