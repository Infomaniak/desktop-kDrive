/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
using System;
using System.Windows.Forms;

namespace Infomaniak.kDrive
{

    //////////////////////////////////////////////       INTERFACES       //////////////////////////////////////////////

    internal interface IAppConstants
    {
        ISentryConstants Sentry { get; }
        IMatomoConstants Matomo { get; }
        IGitHubConstants GitHub { get; }
        ISyncConstants Sync { get; }
        IDriveConstants Drive { get; }
        IkSuiteConstants kSuite { get; }
        IStorageConstants Storage { get; }
        ILoginConstants Login { get; }
    }

    internal interface ISentryConstants
    {
        string Dsn { get; }
        string Environment { get; }
    }

    internal interface IMatomoConstants
    {
        string Host { get; }
        string SiteId { get; }
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

    internal interface ILoginConstants
    {
        Uri OAtuhRedirectUri { get; }
        Uri OAtuhAuthorizationEndpoint { get; }
        string OAtuhClientId { get; }
    }
    internal interface ISyncConstants
    {
        public NodeId RootNodeId { get; }
        public string RescueFolderName { get; }
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
        public Uri StartFreeUri { get; }
    }
    internal interface IkSuiteConstants
    {
        public Uri HomeUri { get; }
        public Uri TarrifsUri { get; }
        public Uri HelpUri { get; }
    }

    internal sealed class CustomAppConstants : IAppConstants
    {
        public ISentryConstants Sentry { init; get; }
        public IMatomoConstants Matomo { init; get; }
        public IGitHubConstants GitHub { init; get; }
        public IDriveConstants Drive { init; get; }
        public IStorageConstants Storage { init; get; }
        public ISyncConstants Sync { init; get; }
        public ILoginConstants Login { init; get; }
        public IkSuiteConstants kSuite { init; get; }

        public CustomAppConstants(ISentryConstants sentry, IMatomoConstants matomo, IGitHubConstants gitHub, ISyncConstants sync, IDriveConstants drive, IStorageConstants storage, ILoginConstants oAuth, IkSuiteConstants kSuite)
        {
            Sentry = sentry;
            Matomo = matomo;
            GitHub = gitHub;
            Sync = sync;
            Drive = drive;
            Storage = storage;
            Login = oAuth;
            this.kSuite = kSuite;
        }
    }


    //////////////////////////////////////////////       PRODUCTION       //////////////////////////////////////////////
    internal sealed class ProductionSentry : ISentryConstants
    {
        public string Dsn { get; } =
            "https://c6ee7ba768d4f7fcd3a6f787f8cc569e@sentry-desktop.infomaniak.com/5";

        public string Environment { get; } = "production";
    }
    internal sealed class ProductionMatomo : IMatomoConstants
    {
        public string Host { get; } = "https://analytics.infomaniak.com/";
        public string SiteId { get; } = "41";
    }

    internal sealed class ProductionGitHub : IGitHubConstants
    {
        private const string Repo = "https://github.com/Infomaniak/desktop-kDrive";

        public Uri RepoUrl { get; } = new(Repo);
        public Uri LicenseUrl { get; } = new($"{Repo}?tab=GPL-3.0-1-ov-file");
    }

    internal sealed class ProductionSync : ISyncConstants
    {
        public NodeId RootNodeId { get; } = "1";

        public string RescueFolderName { get; } = "kDrive Rescue Folder";
    }


    internal sealed class ProductionDrive : IDriveConstants
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
        public Uri ChangeOfferUri(DriveId? driveId) => new($"https://shop.infomaniak.com/order/drive/{driveId}");
        public Uri StartFreeUri { get; } = new Uri("http://shop.infomaniak.com/order/select/drive");
    }

    internal sealed class ProductionStorage : IStorageConstants
    {
        public Uri DownloadUrl { get; } =
            new("https://download.storage.infomaniak.com/drive/desktopclient");
    }

    internal sealed class ProductionLogin : ILoginConstants
    {
        public Uri OAtuhRedirectUri { get; } = new Uri("kdrive://auth-desktop");
        public Uri OAtuhAuthorizationEndpoint { get; } = new Uri("https://login.infomaniak.com/authorize?skipAutoRedirect=true");
        public string OAtuhClientId { get; } = "5EA39279-FF64-4BB8-A872-4A40B5786317";
    }

    internal sealed class ProductionKSuite : IkSuiteConstants
    {
        public Uri HomeUri { get; } = new Uri("https://www.infomaniak.com/fr/ksuite");
        public Uri TarrifsUri { get; } = new Uri("https://www.infomaniak.com/gtl/myksuite#prices");
        public Uri HelpUri { get; } = new Uri("https://www.infomaniak.com/gtl/support");
    }

    internal sealed class ProductionAppConstants : IAppConstants
    {
        public ISentryConstants Sentry { get; } = new ProductionSentry();
        public IGitHubConstants GitHub { get; } = new ProductionGitHub();
        public ISyncConstants Sync { get; } = new ProductionSync();
        public IDriveConstants Drive { get; } = new ProductionDrive();
        public IStorageConstants Storage { get; } = new ProductionStorage();
        public ILoginConstants Login { get; } = new ProductionLogin();
        public IkSuiteConstants kSuite { get; } = new ProductionKSuite();
    }


    //////////////////////////////////////////////       PREPROD       //////////////////////////////////////////////

    internal sealed class PreProdLogin : ILoginConstants
    {
        public Uri OAtuhRedirectUri { get; } = new ProductionLogin().OAtuhRedirectUri;
        public Uri OAtuhAuthorizationEndpoint { get; } = new Uri("https://login.preprod.dev.infomaniak.ch/authorize?skipAutoRedirect=true");
        public string OAtuhClientId { get; } = new ProductionLogin().OAtuhClientId;
    }

    internal sealed class PreProdAppConstants : IAppConstants
    {
        public ISentryConstants Sentry { get; } = new ProductionSentry();
        public IGitHubConstants GitHub { get; } = new ProductionGitHub();
        public ISyncConstants Sync { get; } = new ProductionSync();

        public IDriveConstants Drive { get; } = new ProductionDrive();
        public IStorageConstants Storage { get; } = new ProductionStorage();
        public ILoginConstants Login { get; } = new PreProdLogin();
        public IkSuiteConstants kSuite { get; } = new ProductionKSuite();
    }

}