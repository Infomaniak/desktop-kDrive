using System;

namespace Infomaniak.kDrive
{
    internal interface IAppConstants
    {
        ISentryConstants Sentry { get; }
        IGitHubConstants GitHub { get; }
        IDriveConstants Drive { get; }
        IStorageConstants Storage { get; }
    }

    internal interface ISentryConstants
    {
        string Dsn { get; }
        string Environment { get; }
    }

    internal interface IGitHubConstants
    {
        Uri RepoUrl { get; }
        Uri LicenseUrl { get; }
    }

    internal interface IStorageConstants
    {
        Uri DownloadUrl { get; }
        Uri ReleaseNoteUrl(string versionTag, string languageCode) =>
            new($"{DownloadUrl}/kDrive-{versionTag}-win-{languageCode}.html");
    }


    internal interface IDriveConstants
    {
        Uri RenewUrl(DriveId? driveId);
        Uri kSuiteHomeUrl(DriveId? driveId);
        Uri kDriveHomeUrl(DriveId? driveId);
        Uri TrashUrl(DriveId? driveId);
        Uri FavoriteUrl(DriveId? driveId);
        Uri SharedUrl(DriveId? driveId);
        Uri itemUri(DriveId? driveId, NodeId nodeId);
        Uri ChangeOfferUri(DriveId? driveId);
    }

    internal sealed class ProductionAppConstants : IAppConstants
    {
        public ISentryConstants Sentry { get; } = new ProductionSentry();
        public IGitHubConstants GitHub { get; } = new ProductionGitHub();
        public IDriveConstants Drive { get; } = new ProductionDrive();
        public IStorageConstants Storage { get; } = new ProductionStorage();

        private sealed class ProductionSentry : ISentryConstants
        {
            public string Dsn { get; } =
                "https://c6ee7ba768d4f7fcd3a6f787f8cc569e@sentry-desktop.infomaniak.com/5";

            public string Environment { get; } = "production";
        }

        private sealed class ProductionGitHub : IGitHubConstants
        {
            private const string Repo = "https://github.com/Infomaniak/desktop-kDrive";

            public Uri RepoUrl { get; } = new(Repo);
            public Uri LicenseUrl { get; } = new($"{Repo}?tab=GPL-3.0-1-ov-file");
        }

        private sealed class ProductionDrive : IDriveConstants
        {
            public Uri RenewUrl(DriveId? driveId) =>
                new($"https://shop.infomaniak.com/order/drive/{driveId}");
            public Uri kSuiteHomeUrl(DriveId? driveId) =>
                new($"https://ksuite.infomaniak.com/kdrive/app/drive/{driveId}");
            public Uri kDriveHomeUrl(DriveId? driveId) =>
                new($"https://kdrive.infomaniak.com/app/drive/{driveId}");
            public Uri TrashUrl(DriveId? driveId) => new($"{kSuiteHomeUrl(driveId)}/trash");
            public Uri FavoriteUrl(DriveId? driveId) => new($"{kSuiteHomeUrl(driveId)}/favorites");
            public Uri SharedUrl(DriveId? driveId) => new($"{kSuiteHomeUrl(driveId)}/shared-with-me");
            public Uri itemUri(DriveId? driveId, NodeId nodeId) => new($"{kDriveHomeUrl(driveId)}/redirect/{nodeId}");
            public Uri ChangeOfferUri(DriveId? driveId) =>
                new($"https://shop.infomaniak.com/order/drive/{driveId}");
        }
        private sealed class ProductionStorage : IStorageConstants
        {
            public Uri DownloadUrl { get; } =
                new("https://download.storage.infomaniak.com/drive/desktopclient");
        }
    }

}
