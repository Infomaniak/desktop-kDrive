using Infomaniak.kDrive.Analytics;
using System.Runtime.InteropServices;

namespace Infomaniak.kDrive.Tests;

public class UserAgentBuilderTests
{
    [Fact]
    public void BuildDesktopUserAgent_Windows11X64_ReportsWindowsNt10()
    {
        var info = new UserAgentInfo("kDrive", "4.0.1", "10.0.22631", UserAgentArchitecture.X64);

        Assert.Equal("Mozilla/5.0 (Windows NT 10.0; Win64; x64) kDrive/4.0.1", UserAgentBuilder.BuildDesktopUserAgent(info));
    }

    [Theory]
    [InlineData(Architecture.Arm64, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) kDrive/4.0.1")]
    [InlineData(Architecture.Arm, "Mozilla/5.0 (Windows NT 10.0; ARM) kDrive/4.0.1")]
    [InlineData(Architecture.X86, "Mozilla/5.0 (Windows NT 10.0) kDrive/4.0.1")]
    [InlineData(Architecture.Wasm, "Mozilla/5.0 (Windows NT 10.0) kDrive/4.0.1")]
    public void BuildDesktopUserAgent_UsesBrowserArchitectureTokens(Architecture architecture, string expected)
    {
        var info = new UserAgentInfo("kDrive", "4.0.1", "10.0.26100", UserAgentBuilder.ArchitectureFrom(architecture));

        Assert.Equal(expected, UserAgentBuilder.BuildDesktopUserAgent(info));
    }

    [Theory]
    [InlineData("")]
    [InlineData("not a version")]
    public void BuildDesktopUserAgent_MissingOsVersion_FallsBackToWindowsNt10(string osVersion)
    {
        var info = new UserAgentInfo("kDrive", "4.0.1", osVersion, UserAgentArchitecture.X64);

        Assert.Equal("Mozilla/5.0 (Windows NT 10.0; Win64; x64) kDrive/4.0.1", UserAgentBuilder.BuildDesktopUserAgent(info));
    }

    [Fact]
    public void BuildDesktopUserAgent_SanitizesProductToken()
    {
        var info = new UserAgentInfo("  k Drive (beta)/ ", " 4.0.1 rc-1;", "6.3.9600", UserAgentArchitecture.X64);

        Assert.Equal("Mozilla/5.0 (Windows NT 6.3; Win64; x64) k-Drive-beta/4.0.1-rc-1", UserAgentBuilder.BuildDesktopUserAgent(info));
    }

    [Fact]
    public void BuildDesktopUserAgent_EmptyVersion_OmitsVersion()
    {
        var info = new UserAgentInfo("kDrive", "", "10.0", UserAgentArchitecture.X64);

        Assert.Equal("Mozilla/5.0 (Windows NT 10.0; Win64; x64) kDrive", UserAgentBuilder.BuildDesktopUserAgent(info));
    }

    [Fact]
    public void BuildDesktopUserAgent_EmptyProductName_ReturnsEmpty()
    {
        var info = new UserAgentInfo(" ", "4.0.1", "10.0", UserAgentArchitecture.X64);

        Assert.Equal(string.Empty, UserAgentBuilder.BuildDesktopUserAgent(info));
    }

    [Theory]
    [InlineData("15.0.0", "15.0.0")]
    [InlineData("10.0.0", "10.0.0")]
    [InlineData(" 19.0.0. ", "19.0.0")]
    public void BuildClientHints_ReportsWindowsPlatformVersion(string platformVersion, string expectedPlatformVersion)
    {
        var info = new UserAgentInfo("kDrive", "4.0.1", "10.0.26100", UserAgentArchitecture.X64, platformVersion);

        Assert.Equal($$"""{"platform":"Windows","platformVersion":"{{expectedPlatformVersion}}","mobile":false,"model":""}""",
            UserAgentBuilder.BuildClientHints(info));
    }

    [Theory]
    [InlineData("")]
    [InlineData("unknown")]
    public void BuildClientHints_MissingPlatformVersion_ReturnsEmpty(string platformVersion)
    {
        var info = new UserAgentInfo("kDrive", "4.0.1", "10.0.26100", UserAgentArchitecture.X64, platformVersion);

        Assert.Equal(string.Empty, UserAgentBuilder.BuildClientHints(info));
    }
}
