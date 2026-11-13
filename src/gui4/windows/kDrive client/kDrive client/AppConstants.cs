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

namespace Infomaniak.kDrive
{

    //////////////////////////////////////////////       INTERFACES       //////////////////////////////////////////////

    internal interface IAppConstants
    {
        ISentryConstants Sentry { get; }
        IMatomoConstants Matomo { get; }
        ISyncConstants Sync { get; }
        IkSuiteConstants kSuite { get; }
        ILoginConstants Login { get; }
    }

    internal interface ISentryConstants
    {
        string Dsn { get; }
    }

    internal interface IMatomoConstants
    {
        string Host { get; }
        string SiteId { get; }
    }

    internal interface ILoginConstants
    {
        Uri OAuthRedirectUri { get; }
        string OAuthClientId { get; }
    }
    
    internal interface ISyncConstants
    {
        public NodeId RootNodeId { get; }
        public string RescueFolderName { get; }
    }

    internal interface IkSuiteConstants
    {
        public Uri TarrifsUri { get; }
        public Uri HelpUri { get; }
    }

    internal sealed class CustomAppConstants : IAppConstants
    {
        public ISentryConstants Sentry { init; get; }
        public IMatomoConstants Matomo { init; get; }
        public ISyncConstants Sync { init; get; }
        public ILoginConstants Login { init; get; }
        public IkSuiteConstants kSuite { init; get; }

        public CustomAppConstants(ISentryConstants sentry, IMatomoConstants matomo, ISyncConstants sync, ILoginConstants oAuth, IkSuiteConstants kSuite)
        {
            Sentry = sentry;
            Matomo = matomo;
            Sync = sync;
            Login = oAuth;
            this.kSuite = kSuite;
        }
    }


    //////////////////////////////////////////////       PRODUCTION       //////////////////////////////////////////////
    internal sealed class ProductionSentry : ISentryConstants
    {
        public string Dsn { get; } =
            "https://c6ee7ba768d4f7fcd3a6f787f8cc569e@sentry-desktop.infomaniak.com/5";
    }
    internal sealed class ProductionMatomo : IMatomoConstants
    {
        public string Host { get; } = "https://analytics.infomaniak.com/";
        public string SiteId { get; } = "41";
    }

    internal sealed class ProductionSync : ISyncConstants
    {
        public NodeId RootNodeId { get; } = "1";

        public string RescueFolderName { get; } = "kDrive Rescue Folder";
    }

    internal sealed class ProductionLogin : ILoginConstants
    {
        public Uri OAuthRedirectUri { get; } = new Uri("kdrive://auth-desktop");
        public Uri OAuthAuthorizationEndpoint { get; } = new Uri("https://login.infomaniak.com/authorize?skipAutoRedirect=true");
        public string OAuthClientId { get; } = "5EA39279-FF64-4BB8-A872-4A40B5786317";
    }

    internal sealed class ProductionKSuite : IkSuiteConstants
    {
        public Uri TarrifsUri { get; } = new Uri("https://www.infomaniak.com/gtl/myksuite#prices");
        public Uri HelpUri { get; } = new Uri("https://www.infomaniak.com/gtl/support");
    }

    internal sealed class ProductionAppConstants : IAppConstants
    {
        public ISentryConstants Sentry { get; } = new ProductionSentry();
        public IMatomoConstants Matomo { get; } = new ProductionMatomo();
        public ISyncConstants Sync { get; } = new ProductionSync();
        public ILoginConstants Login { get; } = new ProductionLogin();
        public IkSuiteConstants kSuite { get; } = new ProductionKSuite();
    }
}
