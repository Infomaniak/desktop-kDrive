
using System;

namespace Infomaniak.kDrive
{
    internal interface IAppConstants
    {
        public abstract string SentryDSN { get; }
        public abstract Uri StorageUrl { get; }
        public abstract Uri GitHubRepoUrl { get; }
        public abstract Uri GitHubLicenseUrl { get; }
    }

    class ProductionConstants : IAppConstants
    {
        public string SentryDSN { get; } = "https://c6ee7ba768d4f7fcd3a6f787f8cc569e@sentry-desktop.infomaniak.com/5";
        public Uri StorageUrl { get; } = new Uri("https://download.storage.infomaniak.com/drive/desktopclient");
        public Uri GitHubRepoUrl { get; } = new Uri("https://github.com/Infomaniak/desktop-kDrive");
        public Uri GitHubLicenseUrl { get; } = new Uri("https://github.com/Infomaniak/desktop-kDrive?tab=GPL-3.0-1-ov-file");
    }
}
