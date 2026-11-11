using System;
using System.Collections.ObjectModel;
using System.Threading.Tasks;

namespace Infomaniak.kDrive
{
    interface IUriStore
    {
        async Task<bool> OpenInBrowserAsync(Uri uri)
        {
            try
            {
                var success = await Windows.System.Launcher.LaunchUriAsync(uri);
                return success;
            }
            catch (Exception ex)
            {
                Logger.LogError($"Failed to open URI: {uri}. Exception: {ex.Message}");
                return false;
            }
        }

        // ----------------------------------- Storage URLs -----------------------------------
        Uri ReleaseNoteUrl(string versionTag);


        // ----------------------------------- Login URLs -----------------------------------
        Uri OAuthAuthorizationEndpoint();

        // -----------------------------------  Shop URLs   -----------------------------------
        Uri StartFreeUrl();
        Uri RenewStandAloneDriveUrl(DriveId? driveId);
        Uri RenewKSuiteUrl(KSuiteId? kSuiteId);
        Uri StandAloneDriveUpsellUrl(DriveId? driveId);
        Uri KSuiteUpsellUrl(KSuiteId? kSuiteId);
        Uri StandAloneDriveToKSuiteMigration(DriveId? driveId);

        // ----------------------------------- Github URLs -----------------------------------
        Uri GithubRepoUrl();
        Uri GithubLicenseUrl();

        // ----------------------------------- WebApp URLs -----------------------------------
        Uri WebAppHomeUrl(DriveId? driveId);
        Uri WebAppTrashUrl(DriveId? driveId);
        Uri WebAppFavoritesUrl(DriveId? driveId);
        Uri WebAppSharedUrl(DriveId? driveId);
        Uri WebAppItemUrl(DriveId? driveId, NodeId nodeId);
    }

    class DefaultUriStore : IUriStore
    {
        // ----------------------------------- Storage URLs -----------------------------------
        private static readonly Uri _downloadUrl = new("https://download.storage.infomaniak.com/drive/desktopclient");
        public Uri ReleaseNoteUrl(string versionTag) => new($"{_downloadUrl}/kDrive-{versionTag}-win-{BestLanguageCodeFor(LangueCodeContext.Storage)}.html");  // TODO: Check


        // ----------------------------------- Login URLs -----------------------------------
        private static readonly Uri _productionEndPoint = new("https://login.infomaniak.com/authorize?skipAutoRedirect=true");
        private static readonly Uri _preproductionEndPoint = new("https://login.preprod.dev.infomaniak.ch/authorize?skipAutoRedirect=true");
        public Uri OAuthAuthorizationEndpoint() => Environment.GetEnvironmentVariable("KDRIVE_USE_PREPROD_URL") == "1" ? _preproductionEndPoint : _productionEndPoint;  // TODO: Check


        // ----------------------------------- Shop URLs -----------------------------------
        private static readonly Uri _shopDomainUri = new("https://shop.infomaniak.com");

        //// Shop - Start offer
        public Uri StartFreeUrl() => new($"https://www.infomaniak.com/{BestLanguageCodeFor(LangueCodeContext.InfomaniakSite)}/ksuite/myksuite#prices"); // TODO: Check


        //// Shop - Renew
        public Uri RenewStandAloneDriveUrl(DriveId? driveId) => new($"{_shopDomainUri}/order/drive/{driveId}"); // TODO: Check 
        public Uri RenewKSuiteUrl(KSuiteId? kSuiteId) => new($"{_shopDomainUri}/order/drive/{kSuiteId}"); // TODO: Check

        //// Shop - Upsell
        public Uri StandAloneDriveUpsellUrl(DriveId? driveId) => RenewStandAloneDriveUrl(driveId); // TODO: Check
        public Uri KSuiteUpsellUrl(KSuiteId? kSuiteId) => RenewKSuiteUrl(kSuiteId); // TODO: Check
        public Uri StandAloneDriveToKSuiteMigration(DriveId? driveId) => new($"{_shopDomainUri}/order/ksuite?migration=drive&drive_id={driveId}&lang={BestLanguageCodeFor(LangueCodeContext.InfomaniakSite)}"); // TODO: Check


        // ----------------------------------- Github URLs -----------------------------------
        private static readonly Uri _githubRepoUrl = new("https://github.com/Infomaniak/desktop-kDrive");
        public Uri GithubRepoUrl() => _githubRepoUrl;
        public Uri GithubLicenseUrl() => new($"{_githubRepoUrl}?tab=GPL-3.0-1-ov-file");


        // ----------------------------------- WebApp URLs -----------------------------------
        private static readonly Uri _kDriveWebAppUrl = new("https://ksuite.infomaniak.com/kdrive/app/drive");
        public Uri WebAppHomeUrl(DriveId? driveId) => new($"{_kDriveWebAppUrl}/{driveId}");
        public Uri WebAppTrashUrl(DriveId? driveId) => new($"{_kDriveWebAppUrl}/{driveId}/trash");
        public Uri WebAppFavoritesUrl(DriveId? driveId) => new($"{_kDriveWebAppUrl}/{driveId}/favorites");
        public Uri WebAppSharedUrl(DriveId? driveId) => new($"{_kDriveWebAppUrl}/{driveId}/shared-with-me");
        public Uri WebAppItemUrl(DriveId? driveId, NodeId nodeId) => new($"{_kDriveWebAppUrl}/{driveId}/redirect/{nodeId}");


        // ----------------------------------- Helpers -----------------------------------
        private static readonly Collection<string> _infomaniakSiteSupportedLanguages = new Collection<string>(new string[] { "en", "fr", "de", "es", "it", "pt", "nl" });
        private static readonly Collection<string> _storageSupportedLanguages = new Collection<string>(new string[] { "en", "fr", "de", "es", "it" });

        private enum LangueCodeContext
        {
            InfomaniakSite,
            Storage
        }

        private string BestLanguageCodeFrom(Collection<string> supported)
        {
            string currentLanguage = Localizer.Instance.CurrentLanguage;
            if (supported.Contains(currentLanguage))
                return currentLanguage;
            else
                return "en";
        }

        private string BestLanguageCodeFor(LangueCodeContext context)
        {
            switch (context)
            {
                case LangueCodeContext.InfomaniakSite:
                    return BestLanguageCodeFrom(_infomaniakSiteSupportedLanguages);
                case LangueCodeContext.Storage:
                    return BestLanguageCodeFrom(_storageSupportedLanguages);
                default:
                    return "en";
            }
        }
    }
}
