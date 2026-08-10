using Infomaniak.kDrive.ServerCommunication.Services;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;

namespace Infomaniak.kDrive.Tests;

public class ClientCertificateTests
{
    /// <summary>
    /// Produces a certificate and its PKCS#1 ("RSA PRIVATE KEY") private key in the same PEM shape the
    /// server writes to the keychain, so the test exercises the exact material the client receives.
    /// </summary>
    private static (string CertificatePem, string PrivateKeyPem) CreateClientPem()
    {
        using var rsa = RSA.Create(2048);
        var request = new CertificateRequest("CN=kdrive-client-localhost", rsa, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
        using var certificate = request.CreateSelfSigned(DateTimeOffset.UtcNow.AddDays(-1), DateTimeOffset.UtcNow.AddYears(1));
        return (certificate.ExportCertificatePem(), rsa.ExportRSAPrivateKeyPem());
    }

    [Fact]
    public void BuildClientCertificate_ReturnsCertificateWithPrivateKey_ForValidPem()
    {
        var (certificatePem, privateKeyPem) = CreateClientPem();

        using X509Certificate2? built = SecureSocketConnection.BuildClientCertificate(certificatePem, privateKeyPem);

        Assert.NotNull(built);
        Assert.True(built!.HasPrivateKey);

        // The rebuilt certificate must keep the same identity so the server can pin/trust it.
        using var expected = X509Certificate2.CreateFromPem(certificatePem);
        Assert.Equal(expected.RawData, built.RawData);
    }

    [Theory]
    [InlineData(null, null)]
    [InlineData("", "")]
    public void BuildClientCertificate_ReturnsNull_WhenMaterialIsMissing(string? certificatePem, string? privateKeyPem)
    {
        Assert.Null(SecureSocketConnection.BuildClientCertificate(certificatePem, privateKeyPem));
    }

    [Fact]
    public void BuildClientCertificate_ReturnsNull_WhenCertificateIsPresentButKeyIsMissing()
    {
        var (certificatePem, _) = CreateClientPem();

        Assert.Null(SecureSocketConnection.BuildClientCertificate(certificatePem, null));
    }

    [Fact]
    public void BuildClientCertificate_ReturnsNull_WhenPemIsInvalid()
    {
        Assert.Null(SecureSocketConnection.BuildClientCertificate("not-a-certificate", "not-a-key"));
    }

    [Fact]
    public void BuildClientCertificate_ReturnsNull_WhenKeyDoesNotMatchCertificate()
    {
        var (certificatePem, _) = CreateClientPem();
        var (_, unrelatedKeyPem) = CreateClientPem();

        Assert.Null(SecureSocketConnection.BuildClientCertificate(certificatePem, unrelatedKeyPem));
    }
}
