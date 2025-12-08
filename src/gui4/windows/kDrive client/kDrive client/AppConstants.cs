
using System;

namespace Infomaniak.kDrive
{
    internal interface IAppConstants
    {
        public abstract Uri StorageUrl { get; }
        public abstract Uri GitHubRepoUrl { get; }
        public abstract Uri GitHubLicenseUrl { get; }
    }

    class ProductionConstants : IAppConstants
    {
        public Uri StorageUrl { get; } = new Uri("https://download.storage.infomaniak.com/drive/desktopclient");
        public Uri GitHubRepoUrl { get; } = new Uri("https://github.com/Infomaniak/desktop-kDrive");
        public Uri GitHubLicenseUrl { get; } = new Uri("https://github.com/Infomaniak/desktop-kDrive?tab=GPL-3.0-1-ov-file");
    }
}
