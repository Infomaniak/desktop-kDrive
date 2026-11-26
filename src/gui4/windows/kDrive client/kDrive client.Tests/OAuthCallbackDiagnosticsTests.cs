namespace Infomaniak.kDrive.Tests;

public class OAuthCallbackDiagnosticsTests
{
    private const string RedirectWithoutState = "kdrive://auth-desktop?code=SUPERSECRET-AUTH-CODE";

    [Fact]
    public void Describe_WithStateAndCode_ReportsBothAndNeverLogsTheCode()
    {
        var callbackUri = new Uri("kdrive://auth-desktop?code=SUPERSECRET-AUTH-CODE&state=AbCdEf0123456789");

        string description = OAuthCallbackDiagnostics.Describe(callbackUri);

        Assert.Contains("state=present (16 chars)", description);
        Assert.Contains("code=present (21 chars)", description);
        Assert.Contains("params=[code, state]", description);
        Assert.DoesNotContain("SUPERSECRET-AUTH-CODE", description);
    }

    [Fact]
    public void Describe_WithoutState_ReportsMissingState()
    {
        var callbackUri = new Uri(RedirectWithoutState);

        string description = OAuthCallbackDiagnostics.Describe(callbackUri);

        Assert.Contains("state=MISSING", description);
    }

    [Fact]
    public void TryGetState_ReadsStateFromQuery()
    {
        Assert.True(OAuthCallbackDiagnostics.TryGetState(new Uri("kdrive://auth-desktop?state=QUERY&code=x"), out string? state));
        Assert.Equal("QUERY", state);
    }

    [Fact]
    public void TryGetState_ReadsStateFromFragment()
    {
        Assert.True(OAuthCallbackDiagnostics.TryGetState(new Uri("kdrive://auth-desktop#state=FRAGMENT&code=x"), out string? state));
        Assert.Equal("FRAGMENT", state);
    }

    [Fact]
    public void ComputeStatePipeName_MatchesWindowsAppSdkSha256Base64()
    {
        // Mirrors WinAppSDK Crypto.h request_pipe_name: "\\.\pipe\oauth\" + standard base64 of sha256(state)
        Assert.Equal(@"\\.\pipe\oauth\jvPis1BlRqF/XbL6JbHJzerXoccLDCn9hhXR1c5HNFY=",
            OAuthCallbackDiagnostics.ComputeStatePipeName("AbCdEf0123456789"));
        Assert.Equal(@"\\.\pipe\oauth\6QePyAzXrqp51O0aUgcGpU4igsSbRYwkbMHwo6NV+Ag=",
            OAuthCallbackDiagnostics.ComputeStatePipeName("TESTSTATE123"));
    }

    [Fact]
    public void DescribeUnparseable_ReportsShapeWithoutLeakingContent()
    {
        string unparseableArg = "kdrive://auth-desktop?code=PARTIALSECRET&garbage=%ZZ";

        string description = OAuthCallbackDiagnostics.DescribeUnparseable(unparseableArg, new UriFormatException("Invalid URI"));

        Assert.Contains("prefix='kdrive://auth-desktop'", description);
        Assert.DoesNotContain("PARTIALSECRET", description);
    }
}
