using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace KDrive.ViewModels.Errors
{
    public class SyncPalError : BaseError
    {
        public SyncPalError(DbId dbId) : base(dbId)
        {
            SolveButton = new ButtonData(GetLocalizedString("Global_ContactSupport")
                , async (sender) =>
            {
                Logger.Log(Logger.Level.Info, "Support button clicked, opening support URL");
                var resourceLoader = Windows.ApplicationModel.Resources.ResourceLoader.GetForViewIndependentUse();
                await Windows.System.Launcher.LaunchUriAsync(new Uri(resourceLoader.GetString("Global_ContactSupport")));
            });

            InfoHyperLink = new ButtonData(GetLocalizedString("Global_MoreInfo"), async (sender) =>
            {
                Logger.Log(Logger.Level.Info, "More info button clicked, opening kDrive FAQ URL");
                var resourceLoader = Windows.ApplicationModel.Resources.ResourceLoader.GetForViewIndependentUse();
                await Windows.System.Launcher.LaunchUriAsync(new Uri(resourceLoader.GetString("Global_FAQUrl")));
            });
        }
        public override sealed string TitleStr()
        {
            return GetLocalizedString("SyncPalError_Title");
        }

        public override string HowToSolveStr()
        {
            return GetLocalizedString("SyncPalError_HowToSolve");
        }

        public override string CauseStr()
        {
            return string.Format(GetLocalizedString("SyncPalError_Cause"), ExitCode, ExitCause);
        }

        public override Uri IconUri()
        {
            return AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "headphones");
        }
    }
}
