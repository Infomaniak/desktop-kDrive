using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Collections.ObjectModel;
using System.Threading.Tasks;
using static System.Net.WebRequestMethods;

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
        Uri RenewDriveUrl(Drive drive);
        Uri DriveUpsellUrl(Drive drive);

        // ----------------------------------- Github URLs -----------------------------------
        Uri GithubRepoUrl();
        Uri GithubLicenseUrl();

        // ----------------------------------- WebApp URLs -----------------------------------
        Uri WebAppHomeUrl(DriveId driveId);
        Uri WebAppTrashUrl(DriveId driveId);
        Uri WebAppFavoritesUrl(DriveId driveId);
        Uri WebAppSharedUrl(DriveId driveId);
        Uri WebAppItemUrl(DriveId driveId, NodeId nodeId);
    }

    class DefaultUriStore : IUriStore
    {
        // ----------------------------------- Storage URLs -----------------------------------
        private static readonly Uri _downloadUrl = new("https://download.storage.infomaniak.com/drive/desktopclient/");
        public Uri ReleaseNoteUrl(string versionTag) => new(_downloadUrl, $"kDrive-{versionTag}-win-{BestLanguageCodeFor(LangueCodeContext.Storage)}.html");


        // ----------------------------------- Login URLs -----------------------------------
        private static readonly Uri _productionEndPoint = new("https://login.infomaniak.com/authorize?skipAutoRedirect=true");
        private static readonly Uri _preproductionEndPoint = new("https://login.preprod.dev.infomaniak.ch/authorize?skipAutoRedirect=true");
        public Uri OAuthAuthorizationEndpoint() => Environment.GetEnvironmentVariable("KDRIVE_USE_PREPROD_URL") == "1" ? _preproductionEndPoint : _productionEndPoint;


        // ----------------------------------- Shop URLs -----------------------------------
        private static readonly Uri _shopDomainUri = new("https://shop.infomaniak.com/");

        //// Shop - Start offer
        public Uri StartFreeUrl() => new($"https://www.infomaniak.com/{BestLanguageCodeFor(LangueCodeContext.InfomaniakSite)}/ksuite/myksuite#prices");


        //// Shop - Renew
        public Uri RenewDriveUrl(Drive drive) => DriveUpsellUrl(drive); // For now, the upsell link is the same as the renew link. This may change in the future.

        //// Shop - Upsell
        public Uri DriveUpsellUrl(Drive drive) => drive.PackUpsellTarget switch
        {
            UpsellTarget.StandAloneToMykSuitePlus => StandAloneDriveToKSuiteMigrationUrl(drive.DriveId),
            UpsellTarget.StandAloneTokSuitePro => StandAloneDriveToKSuiteMigrationUrl(drive.DriveId),
            UpsellTarget.kSuiteUpgrade => KSuiteUpsellUrl(drive.KSuiteId),
            UpsellTarget.MykSuiteToMykSuitePlus => MyKSuiteUpsellUrl(),
            _ => _shopDomainUri
        };

        private Uri StandAloneDriveToKSuiteMigrationUrl(DriveId driveId) => new(_shopDomainUri, $"order/ksuite?migration=drive&drive_id={driveId}&lang={BestLanguageCodeFor(LangueCodeContext.InfomaniakSite)}");
        private Uri KSuiteUpsellUrl(KSuiteId kSuiteId) => new(_shopDomainUri, $"order2/ksuite/{kSuiteId}");
        private Uri MyKSuiteUpsellUrl() => new(_shopDomainUri, $"order/myksuite");

        // ----------------------------------- Github URLs -----------------------------------
        private static readonly Uri _githubRepoUrl = new("https://github.com/Infomaniak/desktop-kDrive");
        public Uri GithubRepoUrl() => _githubRepoUrl;
        public Uri GithubLicenseUrl() => new(_githubRepoUrl, "?tab=GPL-3.0-1-ov-file");


        // ----------------------------------- WebApp URLs -----------------------------------
        private static readonly Uri _kDriveWebAppUrl = new("https://ksuite.infomaniak.com/kdrive/app/drive/");
        public Uri WebAppHomeUrl(DriveId driveId) => new(_kDriveWebAppUrl, $"{driveId}");
        public Uri WebAppTrashUrl(DriveId driveId) => new(_kDriveWebAppUrl, $"{driveId}/trash");
        public Uri WebAppFavoritesUrl(DriveId driveId) => new(_kDriveWebAppUrl, $"{driveId}/favorites");
        public Uri WebAppSharedUrl(DriveId driveId) => new(_kDriveWebAppUrl, $"{driveId}/shared-with-me");
        public Uri WebAppItemUrl(DriveId driveId, NodeId nodeId) => new(_kDriveWebAppUrl, $"{driveId}/redirect/{nodeId}");


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
