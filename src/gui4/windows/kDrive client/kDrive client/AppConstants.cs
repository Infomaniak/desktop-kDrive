
using System;

namespace Infomaniak.kDrive
{
    public interface IAppConstants
    {
        public interface ISentryInfo
        {
            public abstract string DSN { get; }
            public abstract string Environment { get; }
        }
        public abstract ISentryInfo SentryInfo { get; }


        public abstract Uri StorageUrl { get; }
        public abstract Uri GitHubRepoUrl { get; }
        public abstract Uri GitHubLicenseUrl { get; }
    }

    public class ProductionConstants : IAppConstants
    {
        public class ProductionSentryInfo : IAppConstants.ISentryInfo
        {
            public string DSN { get; } = "https://c6ee7ba768d4f7fcd3a6f787f8cc569e@sentry-desktop.infomaniak.com/5";
            public string Environment { get; } = "production";
        }

        public IAppConstants.ISentryInfo SentryInfo => new ProductionSentryInfo();
        public Uri StorageUrl => new Uri("https://download.storage.infomaniak.com/drive/desktopclient");
        public Uri GitHubRepoUrl => new Uri("https://github.com/Infomaniak/desktop-kDrive");
        public Uri GitHubLicenseUrl => new Uri("https://github.com/Infomaniak/desktop-kDrive?tab=GPL-3.0-1-ov-file");

    }
}
